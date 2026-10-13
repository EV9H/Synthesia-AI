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
    void generateMidiBlocking(const MidiGenerationParams& params);
    const juce::MidiBuffer& getGeneratedMidi() const { return generatedMidi; }
    double getClipLengthInBeats() const { return clipLengthBeats; }
    int getClipVersion() const { return clipVersion; }
    // Position within the clip in beats, or a negative value when not playing
    double getPlaybackBeat() const { return playbackBeat.load(); }
    bool isGenerating() const;

    // Preview playback, used when the host transport is not running (e.g. Standalone)
    void setPreviewPlaying(bool shouldPlay);
    bool isPreviewPlaying() const { return previewPlaying.load(); }
    
    // Generator management
    void setGeneratorType(GeneratorType type);
    GeneratorType getCurrentGeneratorType() const { return currentGeneratorType; }
    
    // Settings
    void setApiKey(const juce::String& key);
    juce::String getApiKey() const;

private:
    struct ClipEvent
    {
        double beat;
        juce::MidiMessage message;
    };

    void setGeneratedClip(juce::MidiBuffer newMidi, double lengthInBeats);
    void collectClipEvents(double startBeat, double endBeat, double samplesPerBeat,
                           int numSamples, juce::MidiBuffer& out) const;
    void sendAllNotesOff(juce::MidiBuffer& out);

    // Shared so a background generation keeps its generator alive if the type changes mid-run
    std::shared_ptr<MidiGenerator> generator;
    std::shared_ptr<bool> aliveToken{std::make_shared<bool>(true)};
    GeneratorType currentGeneratorType{GeneratorType::Markov};
    
    juce::MidiBuffer generatedMidi;
    double clipLengthBeats{0.0};
    int clipVersion{0};
    std::atomic<double> playbackBeat{-1.0};

    // Audio-thread copy of the clip, guarded by clipLock
    juce::SpinLock clipLock;
    std::vector<ClipEvent> clipEvents;
    double clipEventsLength{0.0};
    bool clipChanged{false};

    SimpleSynth simpleSynth;
    juce::MidiBuffer blockMidi;
    double currentSampleRate{44100.0};
    double previewBeat{0.0};
    bool wasPlaying{false};
    bool hostWasPlaying{false};
    double nextHostBeat{0.0};
    std::atomic<bool> previewPlaying{false};
    
    std::atomic<bool> isGeneratingMidi{false};
    
    juce::String apiKey;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthesiaAIProcessor)
};
