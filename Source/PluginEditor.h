#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Piano-roll of the generated clip: one colour per part, chord symbols, playback cursor
class ClipView : public juce::Component
{
public:
    void setClip(const juce::MidiBuffer& midi, double lengthInBeats,
                 const std::vector<ChordInfo>& chords,
                 const std::vector<SectionInfo>& sections = {});
    void setPlaybackBeat(double beat);

    void paint(juce::Graphics&) override;

private:
    struct DisplayNote
    {
        int pitch;
        double start;
        double length;
        int velocity;
        int channel;
    };

    std::vector<DisplayNote> notes;
    std::vector<ChordInfo> progression;
    std::vector<SectionInfo> sections;
    double clipLength{0.0};
    double playbackBeat{-1.0};
    int lowestPitch{60};
    int highestPitch{72};
};

class SynthesiaAIEditor : public juce::AudioProcessorEditor,
                          public juce::Timer,
                          public juce::FileDragAndDropTarget
{
public:
    SynthesiaAIEditor(SynthesiaAIProcessor&);
    ~SynthesiaAIEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

private:
    void onGenerateClicked();
    void onPlanClicked();
    void onExportMidi();
    void onImportClicked();
    void onCaptureClicked();
    void onLearnStyleClicked();
    void refreshStyleCombo();
    void learnFromFiles(const juce::Array<juce::File>& files, const juce::String& suggestedName);
    void applyGenreDefaults();
    void applyPlanToUI(const ArrangementPlan& plan);
    void applyImportToUI(const MidiAnalysis& analysis);
    bool importPath(const juce::File& file);
    MidiGenerationParams collectParams() const;
    void updateUIState();
    void exportToChooser(int menuId);

    SynthesiaAIProcessor& audioProcessor;
    
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    
    juce::Label keyLabel;
    juce::ComboBox keyComboBox;
    
    juce::Label scaleLabel;
    juce::ComboBox scaleComboBox;

    juce::Label genreLabel;
    juce::ComboBox genreComboBox;

    juce::Label partLabel;
    juce::ComboBox partComboBox;
    
    juce::Label formLabel;
    juce::ComboBox formComboBox;

    juce::Label lengthLabel;
    juce::Slider lengthSlider;

    juce::Label chordsLabel;
    juce::TextEditor chordsEditor;
    juce::ToggleButton lockProgressionButton;

    juce::Label locksLabel;
    juce::OwnedArray<juce::ToggleButton> partLockButtons;

    juce::Label tempoLabel;
    juce::Slider tempoSlider;
    
    juce::Label tempoSyncLabel;
    juce::ComboBox tempoSyncComboBox;
    
    juce::Label styleLabel;
    juce::TextEditor styleTextEditor;
    
    juce::Label generatorLabel;
    juce::ComboBox generatorComboBox;

    juce::Label importModeLabel;
    juce::ComboBox importModeComboBox;
    juce::TextButton importButton;
    juce::TextButton captureButton;

    juce::Label styleProfileLabel;
    juce::ComboBox styleProfileComboBox;
    juce::Label blendLabel;
    juce::Slider blendSlider;
    juce::TextButton learnStyleButton;
    
    juce::TextButton planButton;
    juce::TextButton generateButton;
    juce::TextButton previewButton;
    juce::TextButton exportButton;

    juce::Label planLabel;
    juce::TextEditor planSummaryEditor;

    ClipView clipView;
    int shownClipVersion{0};
    int shownPlanVersion{0};
    int shownImportVersion{0};
    int shownStyleVersion{0};
    bool draggingFiles{false};
    
    juce::Label statusLabel;
    
    juce::TextButton settingsButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthesiaAIEditor)
};

class SettingsDialog : public juce::Component
{
public:
    SettingsDialog(SynthesiaAIProcessor& p);
    
    void paint(juce::Graphics&) override;
    void resized() override;
    
    juce::String getApiKey() const { return apiKeyEditor.getText(); }

private:
    SynthesiaAIProcessor& processor;
    
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyEditor;
    juce::Label endpointLabel;
    juce::TextEditor endpointEditor;
    juce::Label modelLabel;
    juce::TextEditor modelEditor;
    juce::Label usageLabel;
    juce::Label hintLabel;
    juce::TextButton saveButton;
    juce::TextButton cancelButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialog)
};
