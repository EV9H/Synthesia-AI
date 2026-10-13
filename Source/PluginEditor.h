#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Piano-roll of the generated clip: one colour per part, chord symbols, playback cursor
class ClipView : public juce::Component
{
public:
    void setClip(const juce::MidiBuffer& midi, double lengthInBeats,
                 const std::vector<ChordInfo>& chords);
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
    double clipLength{0.0};
    double playbackBeat{-1.0};
    int lowestPitch{60};
    int highestPitch{72};
};

class SynthesiaAIEditor : public juce::AudioProcessorEditor,
                          public juce::Timer
{
public:
    SynthesiaAIEditor(SynthesiaAIProcessor&);
    ~SynthesiaAIEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void onGenerateClicked();
    void onExportMidi();
    void applyGenreDefaults();
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
    
    juce::Label lengthLabel;
    juce::Slider lengthSlider;

    juce::Label tempoLabel;
    juce::Slider tempoSlider;
    
    juce::Label tempoSyncLabel;
    juce::ComboBox tempoSyncComboBox;
    
    juce::Label styleLabel;
    juce::TextEditor styleTextEditor;
    
    juce::Label generatorLabel;
    juce::ComboBox generatorComboBox;
    
    juce::TextButton generateButton;
    juce::TextButton previewButton;
    juce::TextButton exportButton;

    ClipView clipView;
    int shownClipVersion{0};
    
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
    juce::Label hintLabel;
    juce::TextButton saveButton;
    juce::TextButton cancelButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialog)
};
