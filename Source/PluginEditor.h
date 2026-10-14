#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <functional>

namespace Theme
{
    inline const juce::Colour background{0xff12151c};
    inline const juce::Colour header{0xff1a1f2b};
    inline const juce::Colour panel{0xff1c2230};
    inline const juce::Colour outline{0xff2c3448};
    inline const juce::Colour clipBackground{0xff0e1218};
    inline const juce::Colour accent{0xff5b8def};
    inline const juce::Colour accentAlt{0xff7dcea0};
    inline const juce::Colour label{0xffd5dbe8};
    inline const juce::Colour dimText{0xff8b95a8};
    inline const juce::Colour trial{0xffe0a14a};
    inline const juce::Colour licensed{0xff5fbf8a};
}

class SynthesiaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthesiaLookAndFeel();
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

class ClipView : public juce::Component
{
public:
    void setClip(const juce::MidiBuffer& midi, double lengthInBeats,
                 const std::vector<ChordInfo>& chords,
                 const std::vector<SectionInfo>& sections = {});
    void setPlaybackBeat(double beat);
    void setMuteSolo(int muteMask, int soloMask);

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

    bool noteAudible(int channel) const;

    std::vector<DisplayNote> notes;
    std::vector<ChordInfo> progression;
    std::vector<SectionInfo> sections;
    double clipLength{0.0};
    double playbackBeat{-1.0};
    int lowestPitch{60};
    int highestPitch{72};
    int muteMask{0};
    int soloMask{0};
};

class MidiDragChip : public juce::Component
{
public:
    MidiDragChip(std::function<void()> onDrag, juce::String label);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void setEnabledLook(bool hasMidi);

private:
    std::function<void()> onDrag;
    juce::String label;
    bool hasMidi{false};
    bool dragging{false};
};

class SynthesiaAIEditor : public juce::AudioProcessorEditor,
                          public juce::Timer,
                          public juce::FileDragAndDropTarget,
                          public juce::DragAndDropContainer
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

    void startExternalMidiDrag(Part part);

private:
    void onGenerateClicked();
    void onPlanClicked();
    void onExportMidi();
    void onImportClicked();
    void onCaptureClicked();
    void onLearnStyleClicked();
    void onSavePresetClicked();
    void refreshStyleCombo();
    void refreshPresetCombo();
    void learnFromFiles(const juce::Array<juce::File>& files, const juce::String& suggestedName);
    void applyGenreDefaults();
    void applyPlanToUI(const ArrangementPlan& plan);
    void applyImportToUI(const MidiAnalysis& analysis);
    void applyParamsToUI(const MidiGenerationParams& params);
    void applyPresetFromCombo();
    bool importPath(const juce::File& file);
    MidiGenerationParams collectParams() const;
    void snapshotParams();
    void updateUIState();
    void updateLicenseBadge();
    void exportToChooser(int menuId);
    void layoutGeneratePage();
    void layoutPlanPage();
    void layoutImportPage();
    void layoutStylePage();
    void layoutSoundPage();
    void refreshSoundPatchCombo();
    void pullSoundToUI();
    void pushSoundFromUI();
    Part selectedSoundPart() const;
    void styleControl(juce::Component& c);
    void styleSlider(juce::Slider& slider);
    void applyAdvancedVisibility();
    void onRegeneratePart(Part part, bool variation);

    SynthesiaAIProcessor& audioProcessor;
    SynthesiaLookAndFeel lookAndFeel;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label licenseBadge;
    juce::Label presetLabel;
    juce::ComboBox presetComboBox;
    juce::TextButton savePresetButton;
    juce::TextButton settingsButton;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component generatePage;
    juce::Component planPage;
    juce::Component importPage;
    juce::Component stylePage;
    juce::Component soundPage;

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
    juce::Label tempoLabel;
    juce::Slider tempoSlider;
    juce::Label tempoSyncLabel;
    juce::ComboBox tempoSyncComboBox;
    juce::ToggleButton followHostTempoButton;
    juce::ToggleButton followHostKeyButton;
    juce::Label generatorLabel;
    juce::ComboBox generatorComboBox;
    juce::Label chordsLabel;
    juce::TextEditor chordsEditor;
    juce::ToggleButton lockProgressionButton;
    juce::Label locksLabel;
    juce::OwnedArray<juce::ToggleButton> partLockButtons;
    juce::Label styleLabel;
    juce::TextEditor styleTextEditor;
    juce::TextButton generateButton;
    juce::OwnedArray<juce::Label> mixPartLabels;

    juce::Label planLabel;
    juce::TextEditor planSummaryEditor;
    juce::TextButton planButton;

    juce::Label importModeLabel;
    juce::ComboBox importModeComboBox;
    juce::TextButton importButton;
    juce::TextButton captureButton;
    juce::Label importHintLabel;

    juce::Label styleProfileLabel;
    juce::ComboBox styleProfileComboBox;
    juce::Label blendLabel;
    juce::Slider blendSlider;
    juce::TextButton learnStyleButton;
    juce::Label styleHintLabel;

    juce::Label soundPartLabel;
    juce::ComboBox soundPartCombo;
    juce::Label soundPatchLabel;
    juce::ComboBox soundPatchCombo;
    juce::Label attackLabel;
    juce::Slider attackSlider;
    juce::Label decayLabel;
    juce::Slider decaySlider;
    juce::Label sustainLabel;
    juce::Slider sustainSlider;
    juce::Label releaseLabel;
    juce::Slider releaseSlider;
    juce::Label cutoffLabel;
    juce::Slider cutoffSlider;
    juce::Label resoLabel;
    juce::Slider resoSlider;
    juce::Label driveLabel;
    juce::Slider driveSlider;
    juce::Label reverbSendLabel;
    juce::Slider reverbSendSlider;
    juce::Label delaySendLabel;
    juce::Slider delaySendSlider;
    juce::Label mixSectionLabel;
    juce::Label mixPresetLabel;
    juce::ComboBox mixPresetCombo;
    juce::Label sidechainLabel;
    juce::Slider sidechainSlider;
    juce::Label mixCompLabel;
    juce::Slider mixCompSlider;
    juce::Label soundHintLabel;

    ClipView clipView;
    juce::Label mixLabel;
    juce::OwnedArray<juce::TextButton> muteButtons;
    juce::OwnedArray<juce::TextButton> soloButtons;
    juce::OwnedArray<juce::TextButton> regenButtons;
    juce::OwnedArray<juce::TextButton> variationButtons;
    juce::Label dragHintLabel;
    juce::OwnedArray<MidiDragChip> dragChips;
    juce::TextButton previewButton;
    juce::TextButton exportButton;
    juce::Label statusLabel;

    int shownClipVersion{0};
    int shownPlanVersion{0};
    int shownImportVersion{0};
    int shownStyleVersion{0};
    int shownPresetVersion{0};
    int shownUiParamsVersion{0};
    int shownSoundVersion{-1};
    bool draggingFiles{false};
    bool pullingSound{false};
    bool applyingPreset{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthesiaAIEditor)
};

class SettingsDialog : public juce::Component
{
public:
    SettingsDialog(SynthesiaAIProcessor& p);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void applyProviderDefaults();

    SynthesiaAIProcessor& processor;

    juce::Label providerLabel;
    juce::ComboBox providerComboBox;
    juce::Label apiKeyLabel;
    juce::TextEditor apiKeyEditor;
    juce::Label endpointLabel;
    juce::TextEditor endpointEditor;
    juce::Label modelLabel;
    juce::TextEditor modelEditor;
    juce::ToggleButton advancedToggle;
    juce::Label licenseLabel;
    juce::TextEditor licenseEditor;
    juce::Label usageLabel;
    juce::Label hintLabel;
    juce::TextButton saveButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialog)
};
