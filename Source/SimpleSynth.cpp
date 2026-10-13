#include "SimpleSynth.h"
#include "DrumLibrary.h"
#include "InstrumentLibrary.h"
#include <cmath>
#include <vector>

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;
    constexpr Part pitchedParts[] = { Part::Chords, Part::Bass, Part::Synth, Part::Piano };

    double wrap(double phase)
    {
        return phase - std::floor(phase);
    }

    float noiseSample(juce::uint32& state)
    {
        state = state * 1664525u + 1013904223u;
        return static_cast<float>(static_cast<int>(state >> 16) - 32768) / 32768.0f;
    }

    void splitMidi(const juce::MidiBuffer& in, juce::MidiBuffer* pitched, juce::MidiBuffer& drums)
    {
        for (int i = 0; i < 4; ++i)
            pitched[i].clear();
        drums.clear();

        for (const auto metadata : in)
        {
            const auto message = metadata.getMessage();
            const int ch = message.getChannel();
            if (ch == 10)
                drums.addEvent(message, metadata.samplePosition);
            else
            {
                const int idx = juce::jlimit(0, 3, ch <= 0 ? 2 : ch - 1);
                pitched[idx].addEvent(message, metadata.samplePosition);
            }
        }
    }

    void sanitize(juce::AudioBuffer<float>& buffer, int start, int num)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch, start);
            for (int i = 0; i < num; ++i)
                if (!std::isfinite(data[i]))
                    data[i] = 0.0f;
        }
    }

    float coeffFor(double seconds, double sr)
    {
        return (float) (1.0 - std::exp(-1.0 / (juce::jmax(0.0005, seconds) * sr)));
    }

    void compressStereo(float* left, float* right, int num, float& env,
                        const BusComp& bus, float macro, double sr)
    {
        const float amount = juce::jlimit(0.0f, 1.0f, bus.amount * macro);
        if (amount < 0.02f)
            return;
        const float atk = coeffFor(bus.attack, sr);
        const float rel = coeffFor(bus.release, sr);
        const float ratio = juce::jmax(1.05f, bus.ratio);
        const float thresh = juce::jmax(0.02f, bus.thresh);
        const float makeup = 1.0f + 0.12f * amount * (1.0f - 1.0f / ratio);
        for (int i = 0; i < num; ++i)
        {
            const float s = 0.5f * (std::abs(left[i]) + std::abs(right[i]));
            env += (s > env ? atk : rel) * (s - env);
            float gain = 1.0f;
            if (env > thresh)
            {
                const float over = env / thresh;
                const float compressed = std::pow(over, 1.0f / ratio);
                gain = (thresh * compressed) / (env + 1.0e-8f);
                gain = 1.0f + amount * (gain - 1.0f);
            }
            left[i] *= gain * makeup;
            right[i] *= gain * makeup;
        }
    }

    float onePoleCoeff(double hz, double sr)
    {
        return (float) (1.0 - std::exp(-twoPi * hz / sr));
    }

    void truePeakLimit(float* left, float* right, int num, float& env, double sr)
    {
        const float ceiling = 0.68f; // ~-3.4 dBFS; MP3 encoding adds ~1.5–2 dB true peak
        const float rel = coeffFor(0.16, sr);
        for (int i = 0; i < num; ++i)
        {
            float peak = juce::jmax(std::abs(left[i]), std::abs(right[i]));
            if (i + 1 < num)
            {
                for (int k = 1; k < 8; ++k)
                {
                    const float t = k / 8.0f;
                    const float li = left[i] + t * (left[i + 1] - left[i]);
                    const float ri = right[i] + t * (right[i + 1] - right[i]);
                    peak = juce::jmax(peak, std::abs(li), std::abs(ri));
                }
            }
            if (peak > env)
                env = peak;
            else
                env += rel * (peak - env);
            const float g = env > ceiling ? ceiling / (env + 1.0e-8f) : 1.0f;
            left[i] = juce::jlimit(-ceiling, ceiling, left[i] * g);
            right[i] = juce::jlimit(-ceiling, ceiling, right[i] * g);
        }
    }
}

double PartVoice::polyBlep(double t, double dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt)
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }
    return 0.0;
}

double PartVoice::wrap(double phase)
{
    return phase - std::floor(phase);
}

double PartVoice::waveAt(OscWave wave, double phase, double dt, double pw,
                         double fmRatio, double fmIndex)
{
    switch (wave)
    {
        case OscWave::Sine:
            return std::sin(phase * twoPi);
        case OscWave::Triangle:
        {
            const double t = phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
            return t;
        }
        case OscWave::Saw:
            return 2.0 * phase - 1.0 - polyBlep(phase, dt);
        case OscWave::Square:
        {
            double value = phase < 0.5 ? 1.0 : -1.0;
            value += polyBlep(phase, dt);
            value -= polyBlep(std::fmod(phase + 0.5, 1.0), dt);
            return value;
        }
        case OscWave::Pulse:
        {
            const double width = juce::jlimit(0.05, 0.95, pw);
            double value = phase < width ? 1.0 : -1.0;
            value += polyBlep(phase, dt);
            value -= polyBlep(std::fmod(phase + (1.0 - width), 1.0), dt);
            return value;
        }
        case OscWave::Noise:
            return 0.0; // filled by caller
        case OscWave::Tine:
        {
            const double mod = std::sin(phase * twoPi * fmRatio) * fmIndex;
            return std::sin(phase * twoPi + mod);
        }
    }
    return 0.0;
}

PartVoice::PartVoice()
{
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

bool PartVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PartSound*>(sound) != nullptr;
}

void PartVoice::startNote(int midiNoteNumber, float vel,
                          juce::SynthesiserSound* sound, int pitchWheel)
{
    auto* partSound = static_cast<PartSound*>(sound);
    patch = &partSound->patch;
    midiNote = midiNoteNumber;
    velocity = vel;

    const double sampleRate = getSampleRate();
    if (sampleRate != preparedSampleRate)
    {
        filter.prepare({ sampleRate, 512, 1 });
        preparedSampleRate = sampleRate;
    }
    filter.reset();
    ampEnvelope.setSampleRate(sampleRate);
    filtEnvelope.setSampleRate(sampleRate);
    ampEnvelope.setParameters(patch->amp);
    filtEnvelope.setParameters(patch->filterEnv);

    switch (patch->filterMode)
    {
        case FilterMode::HighPass:
            filter.setType(juce::dsp::StateVariableTPTFilterType::highpass);
            break;
        case FilterMode::BandPass:
            filter.setType(juce::dsp::StateVariableTPTFilterType::bandpass);
            break;
        default:
            filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
            break;
    }
    filter.setResonance(juce::jlimit(0.05f, 0.95f, 0.12f + patch->resonance * 0.75f));

    noteHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    targetHz = noteHz;
    if (patch->glideSeconds < 0.001 || currentHz < 1.0)
        currentHz = noteHz;
    updateBend(pitchWheel);

    noteTime = 0.0;
    lfoPhase = 0.0;
    wowPhase = 0.13;
    noiseState = 1u + static_cast<juce::uint32>(midiNoteNumber * 997 + static_cast<int>(vel * 8000));
    subPhase = 0.0;
    fmPhase = 0.17;
    for (int i = 0; i < kMaxUnison; ++i)
    {
        osc1Phase[i] = wrap(0.11 * (i + 1));
        osc2Phase[i] = wrap(0.27 * (i + 3));
    }

    instSample = nullptr;
    instPos = 0.0;
    if (patch->sampleKind != SampleKind::Off)
    {
        InstrumentLibrary::get().ensureLoaded();
        instSample = InstrumentLibrary::get().find(patch->sampleKind, midiNoteNumber, vel);
    }

    ampEnvelope.reset();
    filtEnvelope.reset();
    ampEnvelope.noteOn();
    filtEnvelope.noteOn();
}

void PartVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        ampEnvelope.noteOff();
        filtEnvelope.noteOff();
    }
    else
    {
        ampEnvelope.reset();
        filtEnvelope.reset();
        clearCurrentNote();
    }
}

void PartVoice::pitchWheelMoved(int newValue)
{
    if (isKeyDown())
        updateBend(newValue);
}

void PartVoice::updateBend(int wheelValue)
{
    const double range = (patch != nullptr && patch->part == Part::Bass) ? 12.0 : 2.0;
    const double semitones = (wheelValue - 8192) / 8192.0 * range;
    bendRatio = std::pow(2.0, semitones / 12.0);
}

double PartVoice::oscSample(const OscSettings& osc, double* phases, double hz, double dtBase, double pw) const
{
    juce::ignoreUnused(dtBase);
    const int n = juce::jlimit(1, kMaxUnison, osc.unison);
    const double octaveMul = std::pow(2.0, osc.octave);
    double sum = 0.0;
    const double spread = n > 1 ? osc.unisonDetune : 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double offset = n == 1 ? osc.detuneCents
                                     : osc.detuneCents + (i - (n - 1) * 0.5) * (spread / std::max(1, n / 2));
        const double voiceHz = hz * octaveMul * std::pow(2.0, offset / 1200.0);
        const double dt = voiceHz / getSampleRate();
        if (osc.wave == OscWave::Noise)
            sum += 0.0;
        else
            sum += waveAt(osc.wave, phases[i], dt, pw, osc.fmRatio, osc.fmIndex);
        phases[i] = wrap(phases[i] + dt);
    }
    return (sum / static_cast<double>(n)) * osc.mix;
}

float PartVoice::readInstSample(double hz)
{
    if (instSample == nullptr)
        return 0.0f;

    const auto& buf = instSample->data;
    const int n = buf.getNumSamples();
    if (n < 2)
        return 0.0f;

    const float* d = buf.getReadPointer(0);
    const double sr = getSampleRate();
    const double transpose = std::pow(2.0, (midiNote - instSample->rootNote) / 12.0);
    const double inc = (instSample->sourceRate / juce::jmax(1.0, sr)) * transpose
                     * (hz / juce::jmax(1.0, noteHz));

    auto lerp = [&](double pos) -> float
    {
        if (pos < 0.0 || pos >= n - 1)
            return 0.0f;
        const int i0 = (int) pos;
        const float frac = (float) (pos - i0);
        return d[i0] + frac * (d[i0 + 1] - d[i0]);
    };

    double pos = instPos;
    float s = 0.0f;
    if (instSample->loopable && isKeyDown())
    {
        const int ls = instSample->loopStart;
        const int le = juce::jmax(ls + 8, instSample->loopEnd);
        const int loopLen = le - ls;
        if (loopLen > 8 && pos >= le)
            pos = ls + std::fmod(pos - ls, (double) loopLen);
        const int xfade = juce::jmin(64, loopLen / 4);
        if (xfade > 4 && pos >= le - xfade)
        {
            const float fade = (float) (le - pos) / (float) xfade;
            s = fade * lerp(pos) + (1.0f - fade) * lerp(ls + (pos - (le - xfade)));
        }
        else
        {
            s = lerp(pos);
        }
    }
    else
    {
        s = lerp(pos);
    }

    instPos = pos + inc;
    return s;
}

void PartVoice::nextStereo(float& left, float& right)
{
    left = right = 0.0f;
    if (patch == nullptr)
        return;

    const double sr = getSampleRate();
    const double glide = patch->glideSeconds;
    if (glide > 0.001)
    {
        const double coeff = 1.0 - std::exp(-1.0 / (glide * sr));
        currentHz += (targetHz - currentHz) * coeff;
    }
    else
    {
        currentHz = targetHz;
    }

    const double pitchEnv = std::exp(-noteTime / std::max(0.008, (double) patch->pitchEnvDecay));
    const double lfoAmp = noteTime < patch->lfoDelay ? 0.0
                          : std::min(1.0, (noteTime - patch->lfoDelay) / 0.25);
    const double lfo = std::sin(lfoPhase * twoPi) * lfoAmp;
    const double wow = patch->wowFlutter * 0.0035 * std::sin(wowPhase * twoPi);

    double hz = currentHz * bendRatio
              * std::pow(2.0, patch->pitchEnvAmount * pitchEnv / 12.0)
              * std::pow(2.0, (patch->lfoToPitch * lfo) / 1200.0)
              * (1.0 + wow);

    const double pw = juce::jlimit(0.05, 0.95, patch->osc1.pulseWidth + patch->lfoToPwm * 0.35 * lfo);
    const double dt = hz / sr;
    const float sampleMix = (patch->sampleKind == SampleKind::Off || instSample == nullptr)
                            ? 0.0f : juce::jlimit(0.0f, 1.0f, patch->sampleMix);

    double value = 0.0;
    if (sampleMix < 0.90f)
    {
        value = oscSample(patch->osc1, osc1Phase, hz, dt, pw);
        value += oscSample(patch->osc2, osc2Phase, hz, dt, pw);

        if (patch->subMix > 0.001f)
        {
            const double subHz = hz * std::pow(2.0, patch->subOctave);
            const double subDt = subHz / sr;
            value += std::sin(subPhase * twoPi) * patch->subMix;
            subPhase = wrap(subPhase + subDt);
        }

        if (patch->noiseMix > 0.001f || patch->tineClick > 0.001f)
        {
            const float n = noiseSample(noiseState);
            value += n * patch->noiseMix;
            value += n * patch->tineClick * std::exp(-noteTime * 90.0);
        }
    }

    if (sampleMix > 0.005f)
    {
        const double sampled = (double) readInstSample(hz) * 1.15;
        value = value * (1.0 - sampleMix) + sampled * sampleMix;
    }

    const float amp = ampEnvelope.getNextSample();
    const float fenv = filtEnvelope.getNextSample();
    if (patch->filterMode != FilterMode::Off)
    {
        const double keyMul = std::pow(2.0, ((midiNote - 60) / 12.0) * patch->keyTrack);
        const double velMul = std::pow(2.0, (velocity - 0.5) * patch->velToCutoff * 2.0);
        const double envMul = std::pow(2.0, fenv * patch->filterEnvAmount + lfo * patch->lfoToFilter);
        const float cutoff = (float) juce::jlimit(40.0, sr * 0.42,
            patch->cutoff * keyMul * velMul * envMul);
        filter.setCutoffFrequency(cutoff);
            value = filter.processSample(0, static_cast<float>(value));
            if (!std::isfinite(value))
                value = 0.0;
    }

    const float drive = 1.0f + 1.15f * patch->drive;
    value = std::tanh(value * drive) / std::tanh(drive);

    const float level = patch->gain * (0.38f + 0.62f * velocity) * 0.22f * amp;
    const float pan = juce::jlimit(-1.0f, 1.0f, patch->pan);
    const float gainL = std::sqrt(0.5f * (1.0f - pan));
    const float gainR = std::sqrt(0.5f * (1.0f + pan));
    left = static_cast<float>(value) * level * gainL;
    right = static_cast<float>(value) * level * gainR;

    lfoPhase = wrap(lfoPhase + patch->lfoRate / sr);
    wowPhase = wrap(wowPhase + 0.65 / sr);
    noteTime += 1.0 / sr;
}

void PartVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    const int chans = outputBuffer.getNumChannels();
    while (--numSamples >= 0)
    {
        float l, r;
        nextStereo(l, r);
        if (chans >= 2)
        {
            outputBuffer.addSample(0, startSample, l);
            outputBuffer.addSample(1, startSample, r);
        }
        else
        {
            outputBuffer.addSample(0, startSample, 0.5f * (l + r));
        }
        ++startSample;
        if (!ampEnvelope.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

SimpleSynth::SimpleSynth()
{
    loadGenreDefaults(Genre::Pop);
    setupVoices();
}

void SimpleSynth::setupVoices()
{
    for (int i = 0; i < 4; ++i)
    {
        pitched[i].clearVoices();
        pitched[i].clearSounds();
        const int voices = (i == 1) ? 8 : 12; // bass fewer
        for (int v = 0; v < voices; ++v)
            pitched[i].addVoice(new PartVoice());
        pitched[i].addSound(new PartSound(pitchedParts[i], patches[i]));
    }

    drums.clearVoices();
    drums.clearSounds();
    for (int i = 0; i < 24; ++i)
        drums.addVoice(new DrumVoice());
    drums.addSound(new DrumSound(currentKit));
}

void SimpleSynth::prepare(double sampleRate, int samplesPerBlock)
{
    DrumLibrary::get().ensureLoaded();
    InstrumentLibrary::get().ensureLoaded();
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    for (auto& s : pitched)
        s.setCurrentPlaybackSampleRate(sampleRate);
    drums.setCurrentPlaybackSampleRate(sampleRate);
    prepareFx(sampleRate, samplesPerBlock);
}

void SimpleSynth::prepareFx(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec{ sampleRate, (juce::uint32) juce::jmax(16, samplesPerBlock), 2 };
    const int n = juce::jmax(samplesPerBlock, 512);
    partBuffer.setSize(2, n);
    musicBus.setSize(2, n);
    bassBus.setSize(2, n);
    drumBus.setSize(2, n);
    delaySend.setSize(2, n);
    reverbSend.setSize(2, n);
    dryBackup.setSize(2, n);

    for (auto& c : chorus)
    {
        c.prepare(spec);
        c.setRate(0.75f);
        c.setDepth(0.35f);
        c.setCentreDelay(7.2f);
        c.setFeedback(0.08f);
        c.setMix(0.35f);
    }

    const int delaySamples = juce::jmax(4, (int) (sampleRate * 2.0) + 8);
    delayBufL.assign((size_t) delaySamples, 0.0f);
    delayBufR.assign((size_t) delaySamples, 0.0f);
    delayWrite = 0;

    reverb.prepare(spec);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.45f;
    rp.damping = 0.38f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters(rp);

    hpStateL = hpStateR = 0.0f;
    shelfStateL = shelfStateR = 0.0f;
    kickLp = kickPulse = scEnv = duckSmooth = 0.0f;
    drumEnv = bassEnv = musicEnv = masterEnv = limitEnv = 0.0f;
    bassMonoLpL = bassMonoLpR = 0.0f;
    haasWrite = 0;
    for (float& s : musicHaas)
        s = 0.0f;
    for (int i = 0; i < 4; ++i)
        partHpL[i] = partHpR[i] = partMudLpL[i] = partMudBpL[i]
            = partMudLpR[i] = partMudBpR[i] = partAirLpL[i] = partAirLpR[i] = 0.0f;
}

void SimpleSynth::releaseResources()
{
    for (auto& s : pitched)
        s.allNotesOff(0, false);
    drums.allNotesOff(0, false);
}

void SimpleSynth::setTempo(double bpm)
{
    tempoBpm = juce::jlimit(40.0, 240.0, bpm);
}

void SimpleSynth::setKit(Genre genre)
{
    if (genre == currentKit)
        return;
    currentKit = genre;
    loadGenreDefaults(genre);
}

void SimpleSynth::loadGenreDefaults(Genre genre)
{
    currentKit = genre;
    patches[0] = PatchLibrary::defaultFor(genre, Part::Chords);
    patches[1] = PatchLibrary::defaultFor(genre, Part::Bass);
    patches[2] = PatchLibrary::defaultFor(genre, Part::Synth);
    patches[3] = PatchLibrary::defaultFor(genre, Part::Piano);
    mix = MixLibrary::defaultFor(genre);
    ++soundVersion;
}

SynthPatch SimpleSynth::getPatch(Part part) const
{
    return patches[PatchLibrary::pitchedIndex(part)];
}

void SimpleSynth::setPatch(Part part, const SynthPatch& patch)
{
    const int i = PatchLibrary::pitchedIndex(part);
    patches[i] = patch;
    patches[i].part = part;
    ++soundVersion;
}

void SimpleSynth::setMix(const MixSettings& settings)
{
    mix = settings;
    ++soundVersion;
}

void SimpleSynth::setSidechainAmount(float amount)
{
    mix.sidechainAmount = juce::jlimit(0.0f, 1.0f, amount);
    ++soundVersion;
}

void SimpleSynth::writeSoundXml(juce::XmlElement& xml) const
{
    xml.setAttribute("kit", static_cast<int>(currentKit));
    for (int i = 0; i < 4; ++i)
    {
        auto* child = xml.createNewChildElement("Patch");
        patches[i].writeXml(*child);
    }
    mix.writeXml(*xml.createNewChildElement("Mix"));
}

void SimpleSynth::readSoundXml(const juce::XmlElement& xml)
{
    currentKit = static_cast<Genre>(juce::jlimit(0, numGenres - 1, xml.getIntAttribute("kit", 0)));
    int i = 0;
    for (auto* child : xml.getChildWithTagNameIterator("Patch"))
    {
        if (i >= 4)
            break;
        patches[i] = SynthPatch::fromXml(*child);
        patches[i].part = pitchedParts[i];
        ++i;
    }
    if (auto* mixXml = xml.getChildByName("Mix"))
        mix = MixSettings::fromXml(*mixXml);
    else
        mix = MixLibrary::defaultFor(currentKit);
    ++soundVersion;
}

void SimpleSynth::ensureAuxSize(int start, int num)
{
    const int needed = start + num;
    auto grow = [needed](juce::AudioBuffer<float>& buf)
    {
        if (buf.getNumChannels() < 2 || buf.getNumSamples() < needed)
            buf.setSize(2, needed, false, false, true);
    };
    grow(partBuffer);
    grow(musicBus);
    grow(bassBus);
    grow(drumBus);
    grow(delaySend);
    grow(reverbSend);
    grow(dryBackup);
}

void SimpleSynth::applyPartInserts(int partIndex, juce::AudioBuffer<float>& buffer, int start, int num)
{
    const auto& patch = patches[partIndex];
    const double sr = juce::jmax(1.0, currentSampleRate);
    auto* l = buffer.getWritePointer(0, start);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, start) : l;
    const bool isBass = partIndex == 1;
    const float hpHz = isBass ? 28.0f : 130.0f;
    const float hpC = onePoleCoeff(hpHz, sr);
    const float mudF = (float) (2.0 * std::sin(twoPi * 360.0 / sr));
    const float mudQ = 0.35f;
    const float mudAmt = isBass ? 0.12f : 0.42f;
    const float airC = onePoleCoeff(7500.0, sr);
    const float airAmt = isBass ? 0.08f : 0.62f;
    const float partScale = isBass ? 0.78f : (partIndex == 0 ? 0.92f : 0.90f);

    for (int i = 0; i < num; ++i)
    {
        auto process = [&](float x, float& hpS, float& mudLp, float& mudBp, float& airLp) -> float
        {
            hpS += hpC * (x - hpS);
            x -= hpS;
            mudLp += mudF * mudBp;
            const float mudHp = x - mudLp - mudQ * mudBp;
            mudBp += mudF * mudHp;
            x -= mudAmt * mudBp;
            airLp += airC * (x - airLp);
            x += airAmt * (x - airLp);
            return x;
        };
        l[i] = process(l[i], partHpL[partIndex], partMudLpL[partIndex], partMudBpL[partIndex], partAirLpL[partIndex]);
        if (r != l)
            r[i] = process(r[i], partHpR[partIndex], partMudLpR[partIndex], partMudBpR[partIndex], partAirLpR[partIndex]);
    }

    if (isBass)
    {
        const float monoC = onePoleCoeff(120.0, sr);
        for (int i = 0; i < num; ++i)
        {
            bassMonoLpL += monoC * (l[i] - bassMonoLpL);
            bassMonoLpR += monoC * (r[i] - bassMonoLpR);
            const float mono = 0.5f * (bassMonoLpL + bassMonoLpR);
            l[i] = (l[i] - bassMonoLpL) + mono;
            r[i] = (r[i] - bassMonoLpR) + mono;
        }
    }

    buffer.applyGain(start, num, partScale);

    if (patch.tapeSat > 0.04f)
    {
        const float amount = 1.0f + 0.7f * patch.tapeSat;
        const float inv = 1.0f / std::tanh(amount);
        for (int i = 0; i < num; ++i)
        {
            l[i] = std::tanh(l[i] * amount) * inv;
            r[i] = std::tanh(r[i] * amount) * inv;
        }
    }

    if (patch.chorusSend > 0.02f)
    {
        chorus[partIndex].setMix(juce::jlimit(0.0f, 0.55f, patch.chorusSend * 0.85f));
        juce::dsp::AudioBlock<float> block(buffer);
        auto sub = block.getSubBlock((size_t) start, (size_t) num);
        chorus[partIndex].process(juce::dsp::ProcessContextReplacing<float>(sub));
    }

    sanitize(buffer, start, num);

    const float delayAmt = patch.delaySend * 0.75f;
    const float revAmt = patch.reverbSend * 0.7f;
    if (delayAmt > 0.001f || revAmt > 0.001f)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* src = buffer.getReadPointer(ch, start);
            if (delayAmt > 0.001f)
            {
                float* dst = delaySend.getWritePointer(ch, start);
                for (int i = 0; i < num; ++i)
                    dst[i] += src[i] * delayAmt;
            }
            if (revAmt > 0.001f)
            {
                float* dst = reverbSend.getWritePointer(ch, start);
                for (int i = 0; i < num; ++i)
                    dst[i] += src[i] * revAmt;
            }
        }
    }
}

void SimpleSynth::applyBusMix(int start, int num, const juce::MidiBuffer& drumMidi)
{
    const double sr = juce::jmax(1.0, currentSampleRate);
    const float macro = juce::jlimit(0.0f, 1.0f, mix.compAmount);
    auto* mL = musicBus.getWritePointer(0, start);
    auto* mR = musicBus.getWritePointer(1, start);
    auto* bL = bassBus.getWritePointer(0, start);
    auto* bR = bassBus.getWritePointer(1, start);
    auto* dL = drumBus.getWritePointer(0, start);
    auto* dR = drumBus.getWritePointer(1, start);

    compressStereo(dL, dR, num, drumEnv, mix.drum, macro, sr);
    compressStereo(bL, bR, num, bassEnv, mix.bass, macro, sr);
    compressStereo(mL, mR, num, musicEnv, mix.music, macro, sr);

    const float width = juce::jlimit(1.0f, 2.0f, mix.musicWidth);
    const int haasDelay = currentKit == Genre::LoFiJazz ? 9 : 22;
    const int haasSize = (int) (sizeof(musicHaas) / sizeof(musicHaas[0]));
    for (int i = 0; i < num; ++i)
    {
        const float mid = 0.5f * (mL[i] + mR[i]);
        const float side = 0.5f * (mL[i] - mR[i]) * width;
        mL[i] = mid + side;
        musicHaas[(size_t) haasWrite] = mid - side;
        int rpos = haasWrite - haasDelay;
        if (rpos < 0)
            rpos += haasSize;
        mR[i] = musicHaas[(size_t) rpos];
        if (++haasWrite >= haasSize)
            haasWrite = 0;
    }

    const float scAmt = juce::jlimit(0.0f, 1.0f, mix.sidechainAmount);
    if (scAmt < 0.01f)
        return;

    std::vector<float> kickTrig((size_t) num, 0.0f);
    for (const auto metadata : drumMidi)
    {
        const auto message = metadata.getMessage();
        if (!message.isNoteOn() || message.getVelocity() <= 0)
            continue;
        if (DrumLibrary::pieceFromNote(message.getNoteNumber()) != DrumPiece::Kick)
            continue;
        const int pos = juce::jlimit(0, num - 1, metadata.samplePosition - start);
        kickTrig[(size_t) pos] = juce::jmax(kickTrig[(size_t) pos],
                                            0.25f + 0.55f * message.getFloatVelocity());
    }

    const float atk = coeffFor(juce::jmax(0.008f, mix.sidechainAttack), sr);
    const float rel = coeffFor(juce::jmax(0.08f, mix.sidechainRelease), sr);
    const float pulseDecay = coeffFor(0.045, sr);
    const float smooth = coeffFor(0.008, sr);
    const float lpCoeff = onePoleCoeff(85.0, sr);
    const float bassAmt = juce::jlimit(0.0f, 1.0f, mix.sidechainBass);
    for (int i = 0; i < num; ++i)
    {
        const float mid = 0.5f * (dL[i] + dR[i]);
        kickLp += lpCoeff * (mid - kickLp);
        if (kickTrig[(size_t) i] > kickPulse)
            kickPulse += atk * (kickTrig[(size_t) i] - kickPulse);
        else
            kickPulse += pulseDecay * (0.0f - kickPulse);
        const float key = juce::jmax(std::abs(kickLp) * 2.4f, kickPulse);
        scEnv += (key > scEnv ? atk : rel) * (key - scEnv);
        const float targetDuck = juce::jlimit(0.0f, 0.72f, scEnv * scAmt * 1.15f);
        duckSmooth += smooth * (targetDuck - duckSmooth);
        const float musicG = 1.0f - duckSmooth;
        const float bassG = 1.0f - duckSmooth * bassAmt;
        mL[i] *= musicG;
        mR[i] *= musicG;
        bL[i] *= bassG;
        bR[i] *= bassG;
    }
}

void SimpleSynth::applyMaster(juce::AudioBuffer<float>& buffer, int start, int num)
{
    const double sr = juce::jmax(1.0, currentSampleRate);
    auto* oL = buffer.getWritePointer(0, start);
    auto* oR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, start) : oL;
    auto* sL = delaySend.getWritePointer(0, start);
    auto* sR = delaySend.getWritePointer(1, start);

    if (!delayBufL.empty() && delayBufL.size() == delayBufR.size())
    {
        const int size = (int) delayBufL.size();
        const float delaySec = (float) ((60.0 / juce::jmax(40.0, tempoBpm)) * 0.375);
        int delaySamp = juce::jlimit(1, size - 2, (int) std::round(delaySec * sr));
        int delaySampR = juce::jlimit(1, size - 2, (int) std::round(delaySec * 1.5f * sr));
        const float fb = 0.28f;
        const float wet = 0.2f;
        for (int i = 0; i < num; ++i)
        {
            int rL = delayWrite - delaySamp;
            int rR = delayWrite - delaySampR;
            if (rL < 0) rL += size;
            if (rR < 0) rR += size;
            const float outL = delayBufL[(size_t) rL];
            const float outR = delayBufR[(size_t) rR];
            delayBufL[(size_t) delayWrite] = sL[i] + outR * fb;
            delayBufR[(size_t) delayWrite] = sR[i] + outL * fb;
            oL[i] += outL * wet;
            oR[i] += outR * wet;
            if (++delayWrite >= size)
                delayWrite = 0;
        }
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = currentKit == Genre::Cinematic ? 0.62f
                : currentKit == Genre::LoFiJazz ? 0.38f : 0.44f;
    rp.damping = currentKit == Genre::LoFiJazz ? 0.55f : 0.36f;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = 1.0f;
    reverb.setParameters(rp);

    juce::dsp::AudioBlock<float> revBlock(reverbSend);
    auto revSub = revBlock.getSubBlock((size_t) start, (size_t) num);
    reverb.process(juce::dsp::ProcessContextReplacing<float>(revSub));
    sanitize(reverbSend, start, num);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.addFrom(ch, start, reverbSend, juce::jmin(ch, 1), start, num, 0.72f);

    const float hpCoeff = onePoleCoeff(22.0, sr);
    const float shelfCoeff = onePoleCoeff(9000.0, sr);
    const float shelfGain = currentKit == Genre::LoFiJazz ? 1.22f : 1.48f;
    for (int i = 0; i < num; ++i)
    {
        hpStateL += hpCoeff * (oL[i] - hpStateL);
        hpStateR += hpCoeff * (oR[i] - hpStateR);
        const float hpL = oL[i] - hpStateL;
        const float hpR = oR[i] - hpStateR;
        shelfStateL += shelfCoeff * (hpL - shelfStateL);
        shelfStateR += shelfCoeff * (hpR - shelfStateR);
        oL[i] = hpL + (hpL - shelfStateL) * (shelfGain - 1.0f);
        oR[i] = hpR + (hpR - shelfStateR) * (shelfGain - 1.0f);
    }

    compressStereo(oL, oR, num, masterEnv, mix.master,
                   juce::jlimit(0.0f, 1.0f, mix.compAmount), sr);

    float vinyl = 0.0f;
    float tape = 0.0f;
    for (const auto& p : patches)
    {
        vinyl = juce::jmax(vinyl, p.vinylAmount);
        tape = juce::jmax(tape, p.tapeSat);
    }
    if (currentKit == Genre::LoFiJazz)
        vinyl = juce::jmax(vinyl, 0.08f);

    if (vinyl > 0.01f)
    {
        for (int i = 0; i < num; ++i)
        {
            const float n = noiseSample(vinylState);
            vinylPhase = wrap(vinylPhase + 0.55 / sr);
            const float rumble = 0.008f * std::sin((float) vinylPhase * (float) twoPi);
            const float hiss = n * 0.006f;
            const float crackle = (std::abs(n) > 0.997f) ? n * 0.03f : 0.0f;
            const float v = vinyl * (rumble + hiss + crackle);
            oL[i] += v;
            oR[i] += v * 0.82f;
        }
    }

    if (tape > 0.05f)
    {
        const float amount = 1.0f + 0.55f * tape;
        const float inv = 1.0f / std::tanh(amount);
        for (int i = 0; i < num; ++i)
        {
            oL[i] = std::tanh(oL[i] * amount) * inv;
            oR[i] = std::tanh(oR[i] * amount) * inv;
        }
    }

    const float makeup = juce::jlimit(0.4f, 3.4f, mix.outputGain);
    for (int i = 0; i < num; ++i)
    {
        oL[i] *= makeup;
        oR[i] *= makeup;
    }

    truePeakLimit(oL, oR, num, limitEnv, sr);
    sanitize(buffer, start, num);
}

void SimpleSynth::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                  const juce::MidiBuffer& midiMessages,
                                  int startSample,
                                  int numSamples)
{
    juce::MidiBuffer withChoke;
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOn() && message.getChannel() == 10
            && (DrumLibrary::isClosedHat(message.getNoteNumber())
                || DrumLibrary::isOpenHat(message.getNoteNumber())))
        {
            withChoke.addEvent(juce::MidiMessage::noteOff(10, 46), metadata.samplePosition);
        }
        withChoke.addEvent(message, metadata.samplePosition);
    }

    ensureAuxSize(startSample, numSamples);
    delaySend.clear(startSample, numSamples);
    reverbSend.clear(startSample, numSamples);
    musicBus.clear(startSample, numSamples);
    bassBus.clear(startSample, numSamples);
    drumBus.clear(startSample, numSamples);

    juce::MidiBuffer partMidi[4];
    juce::MidiBuffer drumMidi;
    splitMidi(withChoke, partMidi, drumMidi);

    for (int i = 0; i < 4; ++i)
    {
        partBuffer.clear(startSample, numSamples);
        pitched[i].renderNextBlock(partBuffer, partMidi[i], startSample, numSamples);
        applyPartInserts(i, partBuffer, startSample, numSamples);
        auto& dest = (i == 1) ? bassBus : musicBus;
        for (int ch = 0; ch < 2; ++ch)
            dest.addFrom(ch, startSample, partBuffer, juce::jmin(ch, 1), startSample, numSamples);
    }

    partBuffer.clear(startSample, numSamples);
    drums.renderNextBlock(partBuffer, drumMidi, startSample, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        drumBus.addFrom(ch, startSample, partBuffer, juce::jmin(ch, 1), startSample, numSamples, 0.82f);

    applyBusMix(startSample, numSamples, drumMidi);

    for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
    {
        const int src = juce::jmin(ch, 1);
        outputBuffer.addFrom(ch, startSample, musicBus, src, startSample, numSamples);
        outputBuffer.addFrom(ch, startSample, bassBus, src, startSample, numSamples);
        outputBuffer.addFrom(ch, startSample, drumBus, src, startSample, numSamples);
    }

    dryBackup.copyFrom(0, startSample, outputBuffer, 0, startSample, numSamples);
    if (outputBuffer.getNumChannels() > 1)
        dryBackup.copyFrom(1, startSample, outputBuffer, 1, startSample, numSamples);
    else
        dryBackup.copyFrom(1, startSample, outputBuffer, 0, startSample, numSamples);

    const float dryPeak = outputBuffer.getMagnitude(startSample, numSamples);
    applyMaster(outputBuffer, startSample, numSamples);
    const float wetPeak = outputBuffer.getMagnitude(startSample, numSamples);
    if (dryPeak > 0.02f && (!std::isfinite(wetPeak) || wetPeak < dryPeak * 0.002f))
    {
        outputBuffer.copyFrom(0, startSample, dryBackup, 0, startSample, numSamples);
        if (outputBuffer.getNumChannels() > 1)
            outputBuffer.copyFrom(1, startSample, dryBackup, 1, startSample, numSamples);
    }
}

bool DrumVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<DrumSound*>(sound) != nullptr;
}

float DrumVoice::noise()
{
    noiseState = noiseState * 1664525u + 1013904223u;
    return static_cast<float>(static_cast<int>(noiseState >> 16) - 32768) / 32768.0f;
}

void DrumVoice::pitchWheelMoved(int newValue)
{
    const double semitones = (newValue - 8192) / 8192.0 * 12.0;
    bendRatio = std::pow(2.0, semitones / 12.0);
    if (usingSample)
        updateSampleIncrement();
}

void DrumVoice::updateSampleIncrement()
{
    const double sr = getSampleRate();
    if (sr <= 0.0 || sampleData == nullptr)
        return;
    sampleIncrement = (sampleRateOfFile / sr) * bendRatio * tomRatio;
}

void DrumVoice::startNote(int midiNoteNumber, float velocity,
                          juce::SynthesiserSound* sound, int pitchWheel)
{
    auto* drumSound = dynamic_cast<DrumSound*>(sound);
    kit = drumSound != nullptr ? drumSound->kit : Genre::Pop;
    noiseState = 1u + static_cast<juce::uint32>(midiNoteNumber * 7919 + static_cast<int>(velocity * 10000));
    noteTime = 0.0;
    bodyPhase = 0.0;
    clickPhase = 0.17;
    hpState = lpState = bpState = kitLpState = 0.0;
    env = 1.0;
    honourNoteOff = false;
    usingSample = false;
    sampleData = nullptr;
    samplePos = 0.0;
    tomRatio = 1.0;
    pitchWheelMoved(pitchWheel);

    const float vel = 0.35f + 0.65f * velocity;
    const double sr = getSampleRate();

    auto setDecay = [this, sr](double seconds)
    {
        decayMul = std::exp(-1.0 / std::max(0.01, seconds) / sr);
    };

    kitLpCoeff = 1.0;

    switch (midiNoteNumber)
    {
        case 35: case 36: piece = Piece::Kick; break;
        case 38: case 40: piece = Piece::Snare; break;
        case 39:          piece = Piece::Clap; break;
        case 42: case 44: piece = Piece::ClosedHat; break;
        case 46:          piece = Piece::OpenHat; honourNoteOff = true; break;
        case 49: case 57: piece = Piece::Crash; honourNoteOff = true; break;
        case 51: case 59: piece = Piece::Ride; break;
        case 37: case 54: case 69: case 70: piece = Piece::Perc; break;
        default:          piece = Piece::Tom; break;
    }

    brightPiece = piece == Piece::ClosedHat || piece == Piece::OpenHat
               || piece == Piece::Crash || piece == Piece::Ride || piece == Piece::Perc;
    hatAirLp = 0.0f;
    {
        float pan = 0.0f;
        switch (piece)
        {
            case Piece::Kick:      pan = 0.0f; break;
            case Piece::Snare:     pan = -0.12f; break;
            case Piece::Clap:      pan = 0.16f; break;
            case Piece::ClosedHat: pan = midiNoteNumber == 44 ? -0.32f : -0.62f; break;
            case Piece::OpenHat:   pan = 0.58f; break;
            case Piece::Crash:     pan = 0.28f; break;
            case Piece::Ride:      pan = -0.22f; break;
            case Piece::Perc:      pan = (midiNoteNumber & 1) ? -0.58f : 0.55f; break;
            case Piece::Tom:       pan = 0.10f; break;
        }
        pan += ((int) (noiseState % 9) - 4) * 0.02f;
        pan = juce::jlimit(-0.85f, 0.85f, pan);
        panL = std::sqrt(0.5f * (1.0f - pan));
        panR = std::sqrt(0.5f * (1.0f + pan));
    }

    auto& library = DrumLibrary::get();
    library.ensureLoaded();
    const auto drumPiece = DrumLibrary::pieceFromNote(midiNoteNumber);
    if (const auto* sample = library.find(kit, drumPiece, velocity, midiNoteNumber))
    {
        usingSample = true;
        sampleData = &sample->data;
        sampleRateOfFile = sample->sourceRate;
        if (drumPiece == DrumPiece::Tom)
            tomRatio = std::pow(2.0, (midiNoteNumber - DrumLibrary::tomRootNote()) / 12.0);
        kitLpCoeff = brightPiece ? 1.0 : library.kitLowpassCoeff(kit);
        level = library.gainFor(kit, drumPiece) * vel;
        decayMul = 1.0;
        honourNoteOff = honourNoteOff || drumPiece == DrumPiece::OpenHat
                        || drumPiece == DrumPiece::Crash;
        updateSampleIncrement();
        if (brightPiece)
            sampleIncrement *= 0.972 + 0.056 * ((noiseState >> 9) & 255) / 255.0;
        return;
    }

    switch (kit)
    {
        case Genre::House:
            kickStartHz = 175; kickEndHz = 52; kickPitchRate = 28; kickAmpRate = 11;
            kickClick = 0.55; kickDrive = 1.9; level = 0.16f * vel;
            toneHz = 210; toneMix = 0.25; noiseAmp = 0.7; noiseDecay = 38; bodyDecay = 16;
            kitLpCoeff = 0.90;
            break;
        case Genre::Trap:
            kickStartHz = 110; kickEndHz = 48; kickPitchRate = 22; kickAmpRate = 10;
            kickClick = 0.42; kickDrive = 1.8; level = 0.15f * vel;
            toneHz = 190; toneMix = 0.3; noiseAmp = 0.85; noiseDecay = 42; bodyDecay = 14;
            kitLpCoeff = 0.86;
            break;
        case Genre::LoFiJazz:
            kickStartHz = 125; kickEndHz = 48; kickPitchRate = 16; kickAmpRate = 7;
            kickClick = 0.12; kickDrive = 1.3; level = 0.18f * vel;
            toneHz = 165; toneMix = 0.62; noiseAmp = 0.4; noiseDecay = 18; bodyDecay = 9;
            kitLpCoeff = 0.48;
            break;
        case Genre::RnB:
            kickStartHz = 140; kickEndHz = 44; kickPitchRate = 14; kickAmpRate = 6.5;
            kickClick = 0.22; kickDrive = 1.7; level = 0.16f * vel;
            toneHz = 175; toneMix = 0.5; noiseAmp = 0.55; noiseDecay = 22; bodyDecay = 11;
            kitLpCoeff = 0.58;
            break;
        case Genre::Cinematic:
            kickStartHz = 110; kickEndHz = 38; kickPitchRate = 8; kickAmpRate = 4.2;
            kickClick = 0.18; kickDrive = 1.8; level = 0.18f * vel;
            toneHz = 140; toneMix = 0.55; noiseAmp = 0.5; noiseDecay = 12; bodyDecay = 6;
            kitLpCoeff = 0.62;
            break;
        case Genre::Pop:
        default:
            kickStartHz = 155; kickEndHz = 50; kickPitchRate = 20; kickAmpRate = 8.5;
            kickClick = 0.4; kickDrive = 1.6; level = 0.16f * vel;
            toneHz = 185; toneMix = 0.4; noiseAmp = 0.6; noiseDecay = 28; bodyDecay = 13;
            kitLpCoeff = 0.78;
            break;
    }

    switch (piece)
    {
        case Piece::Kick:
            setDecay(kit == Genre::Cinematic ? 0.9 : 0.32);
            break;
        case Piece::Snare:
            level *= (kit == Genre::House ? 0.85f : 1.0f);
            setDecay(kit == Genre::LoFiJazz ? 0.28 : 0.22);
            break;
        case Piece::Clap:
            level *= 0.9f;
            setDecay(0.28);
            break;
        case Piece::ClosedHat:
            level *= (kit == Genre::House ? 0.45f : 0.38f);
            setDecay(kit == Genre::Trap ? 0.07 : 0.09);
            break;
        case Piece::OpenHat:
            level *= 0.42f;
            setDecay(0.45);
            break;
        case Piece::Ride:
            level *= 0.28f;
            setDecay(0.7);
            break;
        case Piece::Crash:
            level *= 0.22f;
            setDecay(1.8);
            break;
        case Piece::Tom:
            toneHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
            kickStartHz = toneHz * 1.6;
            kickEndHz = toneHz * 0.85;
            kickPitchRate = 12;
            kickAmpRate = 7;
            kickClick = 0.08;
            kickDrive = 1.4;
            level *= 0.24f;
            setDecay(0.4);
            break;
        case Piece::Perc:
            level *= 0.22f;
            setDecay(0.12);
            break;
    }
}

void DrumVoice::stopNote(float, bool allowTailOff)
{
    const double sr = getSampleRate();
    if (honourNoteOff)
    {
        decayMul = std::exp(-1.0 / (allowTailOff ? 0.04 : 0.008) / sr);
        return;
    }

    if (!allowTailOff)
        clearCurrentNote();
}

float DrumVoice::nextSample()
{
    const double sr = getSampleRate();
    double s = 0.0;

    if (usingSample && sampleData != nullptr)
    {
        const int n = sampleData->getNumSamples();
        if (n <= 0 || samplePos >= static_cast<double>(n) || env < 1.0e-4)
        {
            clearCurrentNote();
            return 0.0f;
        }

        const int i0 = juce::jlimit(0, n - 1, static_cast<int>(samplePos));
        const int i1 = juce::jmin(n - 1, i0 + 1);
        const float frac = static_cast<float>(samplePos - i0);
        s = sampleData->getSample(0, i0) * (1.0f - frac) + sampleData->getSample(0, i1) * frac;
        samplePos += sampleIncrement;
        env *= decayMul;
        noteTime += 1.0 / sr;

        if (kitLpCoeff < 0.99 && piece == Piece::Kick)
        {
            kitLpState += kitLpCoeff * (s - kitLpState);
            s = kitLpState;
        }
        if (brightPiece)
        {
            hatAirLp += 0.18f * (static_cast<float>(s) - hatAirLp);
            s += 0.55 * (s - hatAirLp);
        }

        return static_cast<float>(s) * level * static_cast<float>(env);
    }

    return nextSynthSample();
}

float DrumVoice::nextSynthSample()
{
    const double sr = getSampleRate();
    const float n = noise();
    double s = 0.0;

    auto hp = [this](double x, double coeff)
    {
        hpState += coeff * (x - hpState);
        return x - hpState;
    };
    auto lp = [this](double x, double coeff)
    {
        lpState += coeff * (x - lpState);
        return lpState;
    };

    switch (piece)
    {
        case Piece::Kick:
        case Piece::Tom:
        {
            const double f = (kickEndHz + (kickStartHz - kickEndHz) * std::exp(-noteTime * kickPitchRate))
                           * bendRatio;
            bodyPhase += f / sr;
            const double body = std::sin(bodyPhase * twoPi) * std::exp(-noteTime * kickAmpRate);
            const double click = n * std::exp(-noteTime * 220.0);
            s = std::tanh(kickDrive * (body + kickClick * click));
            break;
        }
        case Piece::Snare:
        {
            bodyPhase += toneHz * bendRatio / sr;
            const double body = std::sin(bodyPhase * twoPi) * std::exp(-noteTime * bodyDecay);
            const double snap = hp(n, 0.08) * std::exp(-noteTime * noiseDecay);
            s = toneMix * body + noiseAmp * snap;
            s = std::tanh(1.4 * s);
            break;
        }
        case Piece::Clap:
        {
            auto burst = [&](double delay, double amp)
            {
                const double t = noteTime - delay;
                return t < 0.0 ? 0.0 : n * amp * std::exp(-t * 70.0);
            };
            const double raw = burst(0.0, 1.0) + burst(0.012, 0.7) + burst(0.024, 0.45) + burst(0.041, 0.3);
            s = hp(lp(raw, 0.25), 0.12);
            s = std::tanh(1.5 * s);
            break;
        }
        case Piece::ClosedHat:
        case Piece::OpenHat:
        case Piece::Ride:
        {
            const double metal = std::sin(noteTime * twoPi * 5400.0 * bendRatio)
                               + 0.6 * std::sin(noteTime * twoPi * 7650.0 * bendRatio)
                               + 0.35 * std::sin(noteTime * twoPi * 9200.0 * bendRatio);
            const double air = hp(n, piece == Piece::Ride ? 0.12 : 0.2);
            s = 0.35 * metal + 0.8 * air;
            if (piece == Piece::Ride)
                s = lp(s, 0.35);
            break;
        }
        case Piece::Crash:
        {
            s = hp(n, 0.15) + 0.15 * std::sin(noteTime * twoPi * 4200.0);
            break;
        }
        case Piece::Perc:
        {
            s = hp(n, 0.25) * std::exp(-noteTime * 55.0);
            break;
        }
    }

    env *= decayMul;
    s *= env;

    if (kitLpCoeff < 0.99 && piece == Piece::Kick)
    {
        kitLpState += kitLpCoeff * (s - kitLpState);
        s = kitLpState;
    }
    if (brightPiece)
    {
        hatAirLp += 0.18f * (static_cast<float>(s) - hatAirLp);
        s += 0.5 * (s - hatAirLp);
    }

    noteTime += 1.0 / sr;

    if (env < 1.0e-4 && noteTime > 0.03)
    {
        clearCurrentNote();
        return 0.0f;
    }

    return static_cast<float>(s) * level;
}

void DrumVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    while (--numSamples >= 0)
    {
        const float sample = nextSample();

        if (outputBuffer.getNumChannels() >= 2)
        {
            outputBuffer.addSample(0, startSample, sample * panL);
            outputBuffer.addSample(1, startSample, sample * panR);
        }
        else
        {
            outputBuffer.addSample(0, startSample, sample);
        }

        ++startSample;

        if (!isVoiceActive())
            break;
    }
}
