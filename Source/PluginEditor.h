#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

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
    void updateUIState();

    SynthesiaAIProcessor& audioProcessor;
    
    // UI Components
    juce::Label titleLabel;
    
    juce::Label keyLabel;
    juce::ComboBox keyComboBox;
    
    juce::Label scaleLabel;
    juce::ComboBox scaleComboBox;
    
    juce::Label lengthLabel;
    juce::Slider lengthSlider;
    
    juce::Label tempoSyncLabel;
    juce::ComboBox tempoSyncComboBox;
    
    juce::Label styleLabel;
    juce::TextEditor styleTextEditor;
    
    juce::Label generatorLabel;
    juce::ComboBox generatorComboBox;
    
    juce::TextButton generateButton;
    juce::TextButton exportButton;
    
    juce::Label statusLabel;
    
    // Settings
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
    juce::TextButton saveButton;
    juce::TextButton cancelButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsDialog)
};
