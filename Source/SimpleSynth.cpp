#include "SimpleSynth.h"

// SineWaveVoice implementation
SineWaveVoice::SineWaveVoice() {}

bool SineWaveVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineWaveSound*>(sound) != nullptr;
}

void SineWaveVoice::startNote(int midiNoteNumber, float velocity,
                              juce::SynthesiserSound*, int)
{
    currentAngle = 0.0;
    level = velocity * 0.15;
    tailOff = 0.0;
    
    auto cyclesPerSecond = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    auto cyclesPerSample = cyclesPerSecond / getSampleRate();
    angleDelta = cyclesPerSample * 2.0 * juce::MathConstants<double>::pi;
}

void SineWaveVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        if (tailOff == 0.0)
            tailOff = 1.0;
    }
    else
    {
        clearCurrentNote();
        angleDelta = 0.0;
    }
}

void SineWaveVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                                   int startSample, int numSamples)
{
    if (angleDelta != 0.0)
    {
        if (tailOff > 0.0)
        {
            while (--numSamples >= 0)
            {
                auto currentSample = (float)(std::sin(currentAngle) * level * tailOff);
                
                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);
                
                currentAngle += angleDelta;
                ++startSample;
                
                tailOff *= 0.99;
                
                if (tailOff <= 0.005)
                {
                    clearCurrentNote();
                    angleDelta = 0.0;
                    break;
                }
            }
        }
        else
        {
            while (--numSamples >= 0)
            {
                auto currentSample = (float)(std::sin(currentAngle) * level);
                
                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);
                
                currentAngle += angleDelta;
                ++startSample;
            }
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
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void SimpleSynth::releaseResources()
{
    synth.clearVoices();
    synth.clearSounds();
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
    
    // Add 16 sine wave voices
    for (int i = 0; i < 16; ++i)
        synth.addVoice(new SineWaveVoice());
    
    // Add a sine wave sound
    synth.addSound(new SineWaveSound());
}
