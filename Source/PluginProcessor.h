#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Generators/MidiGenerator.h"
#include "SimpleSynth.h"

class SynthesiaAIProcessor : public juce::AudioProcessor
{
public:
    SynthesiaAIProcessor();
    ~SynthesiaAIProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // MIDI generation
    void generateMidi(const MidiGenerationParams& params);
    const juce::MidiBuffer& getGeneratedMidi() const { return generatedMidi; }
    bool isGenerating() const;
    
    // Generator management
    void setGeneratorType(GeneratorType type);
    GeneratorType getCurrentGeneratorType() const { return currentGeneratorType; }
    
    // Settings
    void setApiKey(const juce::String& key);
    juce::String getApiKey() const;

private:
    std::unique_ptr<MidiGenerator> generator;
    GeneratorType currentGeneratorType{GeneratorType::Markov};
    
    juce::MidiBuffer generatedMidi;
    SimpleSynth simpleSynth;
    
    std::atomic<bool> isGeneratingMidi{false};
    
    juce::String apiKey;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthesiaAIProcessor)
};
