#include "SimpleSynth.h"
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
            level = 0.34f * velocityCurve;
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
    synth.renderNextBlock(outputBuffer, midiMessages, startSample, numSamples);
}

void SimpleSynth::setupVoices()
{
    synth.clearVoices();
    synth.clearSounds();
    
    for (int i = 0; i < 32; ++i)
        synth.addVoice(new PartVoice());

    // Channels match the Part enum: 1 chords, 2 bass, 3 synth, 4 piano; others use the lead.
    synth.addSound(new PartSound(Timbre::Keys, 1, 1));
    synth.addSound(new PartSound(Timbre::Bass, 2, 2));
    synth.addSound(new PartSound(Timbre::Lead, 3, 3));
    synth.addSound(new PartSound(Timbre::Piano, 4, 4));
    synth.addSound(new PartSound(Timbre::Lead, 5, 16));
}
