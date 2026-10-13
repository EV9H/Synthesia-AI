#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Simple sine wave voice
class SineWaveVoice : public juce::SynthesiserVoice
{
public:
    SineWaveVoice();
    
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote(float, bool allowTailOff) override;
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    
private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};

// Simple sine wave sound
class SineWaveSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class SimpleSynth
{
public:
    SimpleSynth();
    ~SimpleSynth() = default;
    
    void prepare(double sampleRate, int samplesPerBlock);
    void releaseResources();
    
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        const juce::MidiBuffer& midiMessages,
                        int startSample,
                        int numSamples);

private:
    juce::Synthesiser synth;
    double currentSampleRate{44100.0};
    
    void setupVoices();
};
