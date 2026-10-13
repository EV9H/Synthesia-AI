#include "SimpleSynth.h"
#include "DrumLibrary.h"
#include <cmath>

namespace
{
    constexpr double twoPi = juce::MathConstants<double>::twoPi;

    // PolyBLEP residual for band-limited saw/square edges
    double polyBlep(double t, double dt)
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

    double saw(double phase, double dt)
    {
        return 2.0 * phase - 1.0 - polyBlep(phase, dt);
    }

    double square(double phase, double dt)
    {
        double value = phase < 0.5 ? 1.0 : -1.0;
        value += polyBlep(phase, dt);
        value -= polyBlep(std::fmod(phase + 0.5, 1.0), dt);
        return value;
    }

    double wrap(double phase)
    {
        return phase - std::floor(phase);
    }

    int bendRangeFor(Timbre timbre)
    {
        return timbre == Timbre::Bass ? 12 : 2;
    }
}

PartVoice::PartVoice()
{
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
}

bool PartVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<PartSound*>(sound) != nullptr;
}

void PartVoice::startNote(int midiNoteNumber, float velocity,
                          juce::SynthesiserSound* sound, int pitchWheel)
{
    timbre = static_cast<PartSound*>(sound)->timbre;

    const double sampleRate = getSampleRate();
    if (sampleRate != preparedSampleRate)
    {
        filter.prepare({ sampleRate, 512, 1 });
        preparedSampleRate = sampleRate;
    }
    filter.reset();
    envelope.setSampleRate(sampleRate);

    frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    phase = 0.0;
    detunedPhase = 0.37;
    vibratoPhase = 0.0;
    noteTime = 0.0;
    updateBend(pitchWheel);

    const float velocityCurve = 0.35f + 0.65f * velocity;

    switch (timbre)
    {
        case Timbre::Keys:
            envelope.setParameters({ 0.02f, 0.35f, 0.7f, 0.35f });
            filter.setCutoffFrequency(static_cast<float>(juce::jmin(2400.0, 900.0 + frequency * 3.0)));
            filter.setResonance(0.6f);
            level = 0.11f * velocityCurve;
            break;
        case Timbre::Bass:
            envelope.setParameters({ 0.004f, 0.5f, 0.85f, 0.09f });
            level = 0.28f * velocityCurve;
            break;
        case Timbre::Lead:
            envelope.setParameters({ 0.005f, 0.25f, 0.6f, 0.16f });
            filter.setCutoffFrequency(3400.0f);
            filter.setResonance(0.9f);
            level = 0.16f * velocityCurve;
            break;
        case Timbre::Piano:
            envelope.setParameters({ 0.002f, 1.8f, 0.0f, 0.25f });
            level = 0.12f * velocityCurve;
            break;
    }

    envelope.reset();
    envelope.noteOn();
}

void PartVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        envelope.noteOff();
    }
    else
    {
        envelope.reset();
        clearCurrentNote();
    }
}

void PartVoice::pitchWheelMoved(int newValue)
{
    // Ignore bends during release so a slide's reset doesn't jump the tail.
    if (isKeyDown())
        updateBend(newValue);
}

void PartVoice::updateBend(int wheelValue)
{
    const double semitones = (wheelValue - 8192) / 8192.0 * bendRangeFor(timbre);
    bendRatio = std::pow(2.0, semitones / 12.0);
}

float PartVoice::nextSample()
{
    const double sampleRate = getSampleRate();
    const double dt = frequency * bendRatio / sampleRate;
    double increment = dt;
    double value = 0.0;

    switch (timbre)
    {
        case Timbre::Keys:
        {
            const double detunedDt = dt * 1.0045;
            value = 0.5 * (saw(phase, dt) + saw(detunedPhase, detunedDt)) + 0.4 * std::sin(phase * twoPi);
            value = filter.processSample(0, static_cast<float>(value));
            detunedPhase = wrap(detunedPhase + detunedDt);
            break;
        }
        case Timbre::Bass:
        {
            const double x = std::sin(phase * twoPi) + 0.2 * std::sin(2.0 * phase * twoPi);
            value = std::tanh(1.8 * x) / std::tanh(1.8);
            break;
        }
        case Timbre::Lead:
        {
            const double vibratoDepth = noteTime > 0.25 ? 0.004 : 0.0;
            const double vibrato = 1.0 + vibratoDepth * std::sin(vibratoPhase * twoPi);
            increment = dt * vibrato;
            value = 0.6 * saw(phase, increment) + 0.4 * square(phase, increment);
            value = filter.processSample(0, static_cast<float>(value));
            vibratoPhase = wrap(vibratoPhase + 5.5 / sampleRate);
            break;
        }
        case Timbre::Piano:
        {
            const double angle = phase * twoPi;
            value = std::sin(angle)
                  + 0.45 * std::exp(-noteTime / 0.6) * std::sin(2.0 * angle)
                  + 0.22 * std::exp(-noteTime / 0.35) * std::sin(3.0 * angle)
                  + 0.10 * std::exp(-noteTime / 0.2) * std::sin(4.0 * angle)
                  + 0.05 * std::exp(-noteTime / 0.1) * std::sin(5.0 * angle);
            break;
        }
    }

    phase = wrap(phase + increment);
    noteTime += 1.0 / sampleRate;

    return static_cast<float>(value) * level * envelope.getNextSample();
}

void PartVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                int startSample, int numSamples)
{
    if (!isVoiceActive())
        return;

    while (--numSamples >= 0)
    {
        const float sample = nextSample();

        for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
            outputBuffer.addSample(i, startSample, sample);

        ++startSample;

        if (!envelope.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

SimpleSynth::SimpleSynth()
{
    setupVoices();
}

void SimpleSynth::prepare(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    DrumLibrary::get().ensureLoaded();
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void SimpleSynth::releaseResources()
{
    synth.allNotesOff(0, false);
}

void SimpleSynth::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                  const juce::MidiBuffer& midiMessages,
                                  int startSample,
                                  int numSamples)
{
    // Closed/pedal hats choke the open hat; retriggering an open hat does too.
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

    synth.renderNextBlock(outputBuffer, withChoke, startSample, numSamples);
}

void SimpleSynth::setKit(Genre genre)
{
    currentKit = genre;
}

void SimpleSynth::setupVoices()
{
    synth.clearVoices();
    synth.clearSounds();
    
    for (int i = 0; i < 32; ++i)
        synth.addVoice(new PartVoice());
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new DrumVoice());

    // Channels 1-4: pitched parts. Channel 10: GM drums. Remaining channels: lead.
    synth.addSound(new PartSound(Timbre::Keys, 1, 1));
    synth.addSound(new PartSound(Timbre::Bass, 2, 2));
    synth.addSound(new PartSound(Timbre::Lead, 3, 3));
    synth.addSound(new PartSound(Timbre::Piano, 4, 4));
    synth.addSound(new PartSound(Timbre::Lead, 5, 9));
    synth.addSound(new DrumSound(currentKit));
    synth.addSound(new PartSound(Timbre::Lead, 11, 16));
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

    // Default dusty lowpass off (coeff 1 = no filtering of the output)
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

    auto& library = DrumLibrary::get();
    library.ensureLoaded();
    const auto drumPiece = DrumLibrary::pieceFromNote(midiNoteNumber);
    if (const auto* sample = library.find(kit, drumPiece, velocity))
    {
        usingSample = true;
        sampleData = &sample->data;
        sampleRateOfFile = sample->sourceRate;
        if (drumPiece == DrumPiece::Tom)
            tomRatio = std::pow(2.0, (midiNoteNumber - DrumLibrary::tomRootNote()) / 12.0);
        kitLpCoeff = library.kitLowpassCoeff(kit);
        level = library.gainFor(kit, drumPiece) * vel;
        decayMul = 1.0;
        honourNoteOff = honourNoteOff || drumPiece == DrumPiece::OpenHat
                        || drumPiece == DrumPiece::Crash;
        updateSampleIncrement();
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
            // Punchy click + short body: the bass part already carries the long 808.
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

        if (kitLpCoeff < 0.99)
        {
            kitLpState += kitLpCoeff * (s - kitLpState);
            s = kitLpState;
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
            // Three delayed noise bursts, band-passed
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

    if (kitLpCoeff < 0.99)
    {
        kitLpState += kitLpCoeff * (s - kitLpState);
        s = kitLpState;
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

        for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
            outputBuffer.addSample(i, startSample, sample);

        ++startSample;

        if (!isVoiceActive())
            break;
    }
}
