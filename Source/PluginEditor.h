#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Inspire.h"
#include <functional>
#include <memory>
#include <vector>

namespace Theme
{
    inline const juce::Colour background{0xff07080d};
    inline const juce::Colour header{0xff10121a};
    inline const juce::Colour surface{0xff141824};
    inline const juce::Colour panel{0xff1a1f2c};
    inline const juce::Colour outline{0x33ffffff};
    inline const juce::Colour clipBackground{0xff0b0d14};
    inline const juce::Colour accent{0xff9a7cff};
    inline const juce::Colour accentViolet{0xff8b6cff};
    inline const juce::Colour accentCoral{0xffff6b5c};
    inline const juce::Colour accentHover{0xffb39cff};
    inline const juce::Colour accentAlt{0xff5ee0c4};
    inline const juce::Colour label{0xfff3f1f7};
    inline const juce::Colour dimText{0xff8d8fa3};
    inline const juce::Colour trial{0xffe0a14a};
    inline const juce::Colour licensed{0xff5fbf8a};

    juce::Font regular(float height);
    juce::Font medium(float height);
    void fillGlass(juce::Graphics& g, juce::Rectangle<float> bounds, float radius);
    void fillAccentGradient(juce::Graphics& g, juce::Rectangle<float> bounds, float alpha = 1.0f);
}

enum class IconKind
{
    Mute, Unmute, Regen, Drag, Play, Stop, Settings, Chevron, Import, Close,
    Dice, Sparkle, Back, Forward, Lock
};

void drawIcon(juce::Graphics& g, IconKind kind, juce::Rectangle<float> bounds, juce::Colour colour);

class SynthesiaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthesiaLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                              bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void fillTextEditorBackground(juce::Graphics&, int, int, juce::TextEditor&) override;
    void drawTextEditorOutline(juce::Graphics&, int, int, juce::TextEditor&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
};

class IconButton : public juce::Button
{
public:
    IconButton(IconKind kind, juce::String tooltip);
    void setKind(IconKind kind);
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    IconKind kind;
};

class Wordmark : public juce::Component
{
public:
    void paint(juce::Graphics&) override;
};

class PreviewOrb : public juce::Component
{
public:
    void setLevel(float level);
    void setPlaying(bool playing);
    void tick();
    void paint(juce::Graphics&) override;

private:
    float level{0.0f};
    float display{0.0f};
    float phase{0.0f};
    bool playing{false};
};

class GenerateButton : public juce::Button
{
public:
    GenerateButton();
    void setBusy(bool busy);
    void tick();
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    bool busy{false};
    float pulse{0.0f};
    float hover{0.0f};
};

class InspireButton : public juce::Button
{
public:
    InspireButton();
    void setBusy(bool busy);
    void tick();
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    bool busy{false};
    float pulse{0.0f};
    float hover{0.0f};
};

class MoodChip : public juce::Button
{
public:
    MoodChip(InspireMood mood);
    InspireMood getMood() const { return mood; }
    void tick();
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    InspireMood mood;
    float hover{0.0f};
};

class CaptionPill : public juce::Component
{
public:
    std::function<void()> onClick;
    void setText(juce::String text);
    void setLocked(bool locked);
    bool isLocked() const { return locked; }
    void tick();
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    juce::String text;
    bool locked{false};
    bool hovered{false};
    float hover{0.0f};
};

class GenreChip : public juce::Button
{
public:
    GenreChip(Genre genre);
    void tick();
    void paintButton(juce::Graphics&, bool highlighted, bool down) override;

private:
    Genre genre;
    float hover{0.0f};
};

class ParamPill : public juce::Component
{
public:
    std::function<void()> onClick;
    void setText(juce::String text);
    void tick();
    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    juce::String text;
    bool hovered{false};
    float hover{0.0f};
};

class DragHandle : public juce::Component
{
public:
    std::function<void()> onDrag;
    void setReady(bool ready);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    bool ready{false};
    bool dragging{false};
    bool hovered{false};
};

class LabeledDrag : public juce::Component
{
public:
    std::function<void()> onDrag;
    void setReady(bool ready);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    bool ready{false};
    bool dragging{false};
    bool hovered{false};
};

class TrackLanes : public juce::Component
{
public:
    std::function<void(Part)> onMute;
    std::function<void(Part)> onRegen;
    std::function<void(Part)> onDrag;

    TrackLanes();
    void setClip(const juce::MidiBuffer& midi, double lengthInBeats,
                 const std::vector<ChordInfo>& chords,
                 const std::vector<SectionInfo>& sections = {});
    void setPlaybackBeat(double beat);
    void setMuteMask(int mask);
    void setReady(bool hasMidi);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct DisplayNote
    {
        int pitch;
        double start;
        double length;
        int velocity;
        int channel;
    };

    struct Lane
    {
        Part part;
        juce::String name;
        juce::Colour colour;
        IconButton mute { IconKind::Unmute, "Mute" };
        IconButton regen { IconKind::Regen, "Regenerate this part" };
        DragHandle drag;
    };

    void paintLaneRoll(juce::Graphics& g, const Lane& lane, juce::Rectangle<float> area) const;

    std::vector<std::unique_ptr<Lane>> lanes;
    std::vector<DisplayNote> notes;
    std::vector<ChordInfo> progression;
    std::vector<SectionInfo> sections;
    double clipLength{0.0};
    double playbackBeat{-1.0};
    int muteMask{0};
    bool hasMidi{false};
};

class ImportModePicker : public juce::Component
{
public:
    std::function<void(ImportMode)> onPick;
    std::function<void()> onCancel;

    ImportModePicker();
    void setFileName(const juce::String& name);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title;
    juce::Label subtitle;
    juce::TextButton accompany { "Accompany" };
    juce::TextButton extend { "Extend" };
    juce::TextButton rewrite { "Rewrite" };
    juce::TextButton restyle { "Restyle" };
    juce::TextButton cancel { "Cancel" };
};

class SettingsOverlay;

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
    void exportToChooser(int menuId);
    void onRegeneratePart(Part part, bool variation);
    void chooseGenre(Genre genre);
    void showKeyMenu();
    void showTempoMenu();
    void showBarsMenu();
    void refreshPills();
    void setMoreOpen(bool open);
    void showImportPicker(const juce::File& file);
    void hideImportPicker();
    void applyImportMode(ImportMode mode);
    void openSettings();
    void hideSettings();
    void styleSlider(juce::Slider& slider);
    void applyEngineChoice();
    void setupSectionLabel(juce::Label& label, const juce::String& text);
    void rebuildBackdrop();
    void onInspireClicked();
    void onInspireAgain();
    void applyInspireIdea(const InspireIdea& idea, bool generate);
    void pushInspireHistory(InspireIdea idea);
    void showInspireHistory(int index);
    void refreshInspireCaption();
    void refreshMoodChips();
    void applyInspireSeed();
    Genre selectedGenre() const;
    ScaleType selectedScale() const;

    SynthesiaAIProcessor& audioProcessor;
    SynthesiaLookAndFeel lookAndFeel;

    Wordmark wordmark;
    juce::Label licenseBadge;
    ParamPill keyPill;
    ParamPill tempoPill;
    ParamPill barsPill;
    IconButton settingsButton { IconKind::Settings, "Settings" };

    juce::TextEditor promptEditor;
    PreviewOrb previewOrb;
    juce::OwnedArray<GenreChip> genreChips;
    juce::OwnedArray<MoodChip> moodChips;
    GenerateButton generateButton;
    InspireButton inspireButton;
    CaptionPill captionGenre;
    CaptionPill captionKey;
    CaptionPill captionTempo;
    CaptionPill captionMood;
    IconButton inspireBack { IconKind::Back, "Previous idea" };
    IconButton inspireForward { IconKind::Forward, "Next idea" };
    juce::TextButton againButton;
    juce::Image backdrop;
    juce::Random inspireRng;
    juce::TextButton importButton;
    juce::TextButton moreButton;
    juce::ComboBox inspireSeedCombo;

    TrackLanes trackLanes;
    LabeledDrag dragAllButton;
    IconButton previewButton { IconKind::Play, "Play / stop preview" };
    juce::Label statusLabel;

    juce::Viewport moreViewport;
    juce::Component morePage;
    juce::Label moreArrangement;
    juce::Label moreHarmony;
    juce::Label moreFeel;
    juce::Label moreStyle;
    juce::Label moreLibrary;

    juce::Label scaleLabel;
    juce::ComboBox scaleComboBox;
    juce::Label formLabel;
    juce::ComboBox formComboBox;
    juce::Label gridLabel;
    juce::ComboBox gridComboBox;
    juce::Label engineLabel;
    juce::ComboBox engineComboBox;
    juce::Label partLabel;
    juce::ComboBox partComboBox;
    juce::Label chordsLabel;
    juce::TextEditor chordsEditor;
    juce::ToggleButton lockProgressionButton;
    juce::Label locksLabel;
    juce::OwnedArray<juce::ToggleButton> partLockButtons;
    juce::TextButton planButton;
    juce::TextEditor planSummaryEditor;
    juce::Label pianoLabel;
    IconButton pianoMute { IconKind::Unmute, "Mute piano" };
    IconButton pianoRegen { IconKind::Regen, "Regenerate piano" };
    DragHandle pianoDrag;
    juce::Label seedLabel;
    juce::Slider seedSlider;
    juce::Label swingLabel;
    juce::Slider swingSlider;
    juce::Label humanizeLabel;
    juce::Slider humanizeSlider;
    juce::Label densityLabel;
    juce::Slider densitySlider;
    juce::Label importModeLabel;
    juce::ComboBox importModeComboBox;
    juce::ToggleButton followHostTempoButton;
    juce::ToggleButton followHostKeyButton;
    juce::Label styleProfileLabel;
    juce::ComboBox styleProfileComboBox;
    juce::Label blendLabel;
    juce::Slider blendSlider;
    juce::TextButton learnStyleButton;
    juce::TextButton captureButton;
    juce::TextButton exportButton;
    juce::Label presetLabel;
    juce::ComboBox presetComboBox;
    juce::TextButton savePresetButton;
    juce::Label varyLabel;
    juce::OwnedArray<juce::TextButton> variationButtons;

    ImportModePicker importPicker;
    std::unique_ptr<SettingsOverlay> settingsOverlay;
    juce::TooltipWindow tooltipWindow { this, 450 };
    juce::File pendingImportFile;
    bool importPickerOpen{false};
    bool settingsOpen{false};

    int keyIndex{0};
    int barsValue{8};
    double tempoValue{110.0};
    Genre genreValue{Genre::Pop};

    int shownClipVersion{0};
    int shownPlanVersion{0};
    int shownImportVersion{0};
    int shownStyleVersion{0};
    int shownPresetVersion{0};
    int shownUiParamsVersion{0};
    bool draggingFiles{false};
    bool applyingPreset{false};
    bool moreOpen{false};
    InspireLocks inspireLocks;
    std::vector<InspireIdea> inspireHistory;
    int inspireIndex{-1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthesiaAIEditor)
};

class SettingsOverlay : public juce::Component
{
public:
    std::function<void()> onClose;

    SettingsOverlay(SynthesiaAIProcessor& p);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;
    void refreshFromProcessor();

private:
    void applyProviderDefaults();
    void save();
    juce::Rectangle<int> cardBounds() const;

    SynthesiaAIProcessor& processor;

    juce::Label title;
    IconButton closeButton { IconKind::Close, "Close" };
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
    juce::TextButton validateLicenseButton;
    juce::Label licenseStatusLabel;
    juce::Label licenseUrlLabel;
    juce::TextEditor licenseUrlEditor;
    juce::Label usageLabel;
    juce::Label hintLabel;
    juce::Label aboutLabel;
    juce::TextButton saveButton;
    juce::TextButton cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsOverlay)
};
