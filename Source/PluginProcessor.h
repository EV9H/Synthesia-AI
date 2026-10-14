#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Generators/MidiGenerator.h"
#include "Generators/ArrangementPlan.h"
#include "MidiImporter.h"
#include "StyleProfile.h"
#include "SimpleSynth.h"
#include "PresetStore.h"
#include "LicenseStub.h"

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

    // MIDI generation. Generating a single part keeps the other parts and, when key, scale,
    // genre and length are unchanged, writes the new part over the existing progression.
    void generateMidi(const MidiGenerationParams& params);
    void generateMidiBlocking(const MidiGenerationParams& params);
    void planArrangement(const MidiGenerationParams& params);
    ArrangementPlan planArrangementBlocking(const MidiGenerationParams& params);
    const juce::MidiBuffer& getGeneratedMidi() const { return generatedMidi; }
    const std::vector<ChordInfo>& getProgression() const { return progression; }
    const std::vector<SectionInfo>& getSections() const { return sections; }
    const ArrangementPlan& getLastPlan() const { return lastPlan; }
    int getPlanVersion() const { return planVersion; }
    TokenUsage getLastTokenUsage() const { return lastPlan.usage; }
    TokenUsage getSessionTokenUsage() const { return sessionUsage; }
    double getClipTempo() const { return previewTempo.load(); }
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
    void setApiEndpoint(const juce::String& url);
    juce::String getApiEndpoint() const { return apiEndpoint; }
    void setApiModel(const juce::String& name);
    juce::String getApiModel() const { return apiModel; }

    bool importMidiFile(const juce::File& file);
    bool importMidiBuffer(const juce::MidiBuffer& midi, double ticksPerQuarter, double tempo,
                          const juce::String& sourceName);
    const MidiAnalysis& getLastImport() const { return lastImport; }
    int getImportVersion() const { return importVersion; }
    void setMidiCapture(bool shouldCapture);
    bool isCapturingMidi() const { return captureEnabled.load(); }
    bool finishMidiCapture();

    bool learnStyle(const juce::Array<juce::File>& files, const juce::String& name);
    void setCurrentStyle(const juce::String& name);
    void setStyleBlend(double amount);
    const StyleProfile& getCurrentStyle() const { return currentStyle; }
    juce::String getCurrentStyleName() const { return currentStyleName; }
    juce::String getCurrentStyleJson() const;
    double getStyleBlend() const { return styleBlend; }
    int getStyleVersion() const { return styleVersion; }
    juce::StringArray listStyleProfiles() const { return StyleProfile::listSaved(); }

    void setLastUiParams(const MidiGenerationParams& params);
    MidiGenerationParams getLastUiParams() const { return lastUiParams; }
    int getUiParamsVersion() const { return uiParamsVersion; }

    bool applyPreset(const juce::String& idOrName);
    bool saveUserPreset(const juce::String& name);
    juce::String getCurrentPresetName() const { return currentPresetName; }
    int getPresetVersion() const { return presetVersion; }

    void setPartMuted(Part part, bool shouldMute);
    void setPartSoloed(Part part, bool shouldSolo);
    bool isPartMuted(Part part) const;
    bool isPartSoloed(Part part) const;
    int getMuteMask() const { return muteMask.load(); }
    int getSoloMask() const { return soloMask.load(); }
    bool isPartAudible(Part part) const;

    juce::File writeTempMidi(Part part) const;

    void regeneratePart(Part part, bool variation);

    void setFollowHostTempo(bool shouldFollow);
    bool getFollowHostTempo() const { return followHostTempo.load(); }
    void setFollowHostKey(bool shouldFollow);
    bool getFollowHostKey() const { return followHostKey.load(); }
    double getHostTempo() const { return hostTempo.load(); }
    int getHostTimeSigNumerator() const { return hostTimeSigNum.load(); }
    int getHostTimeSigDenominator() const { return hostTimeSigDen.load(); }

    void setAdvancedUi(bool enabled);
    bool isAdvancedUi() const;

    void setLicenseKey(const juce::String& key);
    juce::String getLicenseKey() const { return licenseKey; }
    bool isLicensed() const { return LicenseStub::isLicensed(licenseKey); }

    void setApiProvider(const juce::String& name);
    juce::String getApiProvider() const { return apiProvider; }
    static juce::String openaiChatUrl() { return "https://api.openai.com/v1/chat/completions"; }
    static juce::String groqChatUrl() { return "https://api.groq.com/openai/v1/chat/completions"; }
    static juce::String groqDefaultModel() { return "openai/gpt-oss-120b"; }

    void persistLocalSettings() const;
    void loadLocalSettings();

    SimpleSynth& previewSynth() { return simpleSynth; }
    const SimpleSynth& previewSynth() const { return simpleSynth; }
    void setPreviewPatch(Part part, const SynthPatch& patch);
    SynthPatch getPreviewPatch(Part part) const;
    void setPreviewMix(const MixSettings& settings);
    MixSettings getPreviewMix() const;
    void loadGenreSounds(Genre genre);
    int getSoundVersion() const { return simpleSynth.getSoundVersion(); }

private:
    struct ClipEvent
    {
        double beat;
        juce::MidiMessage message;
    };

    struct Result
    {
        juce::MidiBuffer midi;
        std::vector<ChordInfo> progression;
        std::vector<SectionInfo> sections;
        ArrangementPlan plan;
        bool hasPlan{false};
        bool midiReady{true};
        MidiGenerationParams realised;
    };

    MidiGenerationParams prepareParams(MidiGenerationParams params, bool& mergeIntoClip) const;
    Result runGenerator(MidiGenerator& gen, const MidiGenerationParams& params);
    ArrangementPlan createPlan(const MidiGenerationParams& params);
    void applyResult(Result result, const MidiGenerationParams& params, bool mergeIntoClip);
    void storePlan(ArrangementPlan plan, bool bumpUi);
    void configureLlm(MidiGenerator& gen) const;
    void applyImport(MidiAnalysis analysis);
    void setGeneratedClip(juce::MidiBuffer newMidi, double lengthInBeats);
    void collectClipEvents(double startBeat, double endBeat, double samplesPerBeat,
                           int numSamples, juce::MidiBuffer& out) const;
    void sendAllNotesOff(juce::MidiBuffer& out);
    static juce::File localSettingsFile();
    static juce::String completeChatUrl(juce::String url);
    juce::String encodeClipMidi() const;
    bool restoreClipMidi(const juce::String& base64);

    // Shared so a background generation keeps its generator alive if the type changes mid-run
    std::shared_ptr<MidiGenerator> generator;
    std::shared_ptr<bool> aliveToken{std::make_shared<bool>(true)};
    GeneratorType currentGeneratorType{GeneratorType::Genre};
    
    juce::MidiBuffer generatedMidi;
    std::vector<ChordInfo> progression;
    std::vector<SectionInfo> sections;
    MidiGenerationParams clipParams;
    std::atomic<double> previewTempo{120.0};
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
    juce::String apiEndpoint{ "https://api.openai.com/v1/chat/completions" };
    juce::String apiModel{ "gpt-4o-mini" };
    juce::String apiProvider{ "OpenAI" };
    juce::String licenseKey;
    MidiGenerationParams lastUiParams;
    juce::String currentPresetName;
    int currentProgram{0};
    int uiParamsVersion{0};
    int presetVersion{0};
    std::atomic<int> muteMask{0};
    std::atomic<int> soloMask{0};
    ArrangementPlan lastPlan;
    TokenUsage sessionUsage;
    int planVersion{0};
    MidiAnalysis lastImport;
    int importVersion{0};
    StyleProfile currentStyle;
    juce::String currentStyleName;
    double styleBlend{0.0};
    int styleVersion{0};

    std::atomic<bool> followHostTempo{true};
    std::atomic<bool> followHostKey{false};
    std::atomic<double> hostTempo{0.0};
    std::atomic<int> hostTimeSigNum{4};
    std::atomic<int> hostTimeSigDen{4};
    bool advancedUi{false};

    std::atomic<bool> captureEnabled{false};
    juce::SpinLock captureLock;
    struct CaptureEvent { double beat; juce::MidiMessage message; };
    std::vector<CaptureEvent> captureEvents;
    double captureOriginBeat{0.0};
    bool captureHasOrigin{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthesiaAIProcessor)
};
