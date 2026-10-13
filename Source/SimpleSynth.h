#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// One timbre per generated part; parts are routed by MIDI channel.
enum class Timbre
{
    Keys,   // chords: warm detuned saws through a low-pass
    Bass,   // saturated sine, 808-style, +/-12 semitone pitch bend for slides
    Lead,   // synth: saw/square blend with vibrato
    Piano   // decaying additive partials
};

class PartSound : public juce::SynthesiserSound
{
public:
    PartSound(Timbre t, int firstChannel, int lastChannel)
        : timbre(t), lowChannel(firstChannel), highChannel(lastChannel) {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int channel) override { return channel >= lowChannel && channel <= highChannel; }

    const Timbre timbre;

private:
    const int lowChannel;
    const int highChannel;
};

class PartVoice : public juce::SynthesiserVoice
{
public:
    PartVoice();
    
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;
    
private:
    float nextSample();
    void updateBend(int wheelValue);

    Timbre timbre{Timbre::Keys};
    juce::ADSR envelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    double preparedSampleRate{0.0};

    double frequency{440.0};
    double bendRatio{1.0};
    double phase{0.0};
    double detunedPhase{0.0};
    double vibratoPhase{0.0};
    double noteTime{0.0};
    float level{0.0f};
};

class SimpleSynth
{
public:
    static constexpr double releaseSeconds = 0.4;

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
