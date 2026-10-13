#include "SimpleSynth.h"

SineWaveVoice::SineWaveVoice()
{
    envelope.setParameters({ 0.005f, 0.25f, 0.6f, static_cast<float>(SimpleSynth::releaseSeconds) });
}

bool SineWaveVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineWaveSound*>(sound) != nullptr;
}

void SineWaveVoice::startNote(int midiNoteNumber, float velocity,
                              juce::SynthesiserSound*, int)
{
    currentAngle = 0.0;
    level = velocity * 0.18;
    
    auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    auto cyclesPerSample = cyclesPerSecond / getSampleRate();
    angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;

    envelope.setSampleRate(getSampleRate());
    envelope.reset();
    envelope.noteOn();
}

void SineWaveVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        envelope.noteOff();
    }
    else
    {
        envelope.reset();
        clearCurrentNote();
        angleDelta = 0.0;
    }
}

void SineWaveVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                   int startSample, int numSamples)
{
    if (angleDelta == 0.0)
        return;

    while (--numSamples >= 0)
    {
        const auto tone = std::sin(currentAngle)
                        + 0.3 * std::sin(2.0 * currentAngle)
                        + 0.1 * std::sin(3.0 * currentAngle);
        const auto currentSample = static_cast<float>(tone * level * envelope.getNextSample());

        for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
            outputBuffer.addSample(i, startSample, currentSample);

        currentAngle += angleDelta;
        ++startSample;

        if (!envelope.isActive())
        {
            clearCurrentNote();
            angleDelta = 0.0;
            break;
        }
    }
}

// SimpleSynth implementation
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
    
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new SineWaveVoice());
    
    synth.addSound(new SineWaveSound());
}
