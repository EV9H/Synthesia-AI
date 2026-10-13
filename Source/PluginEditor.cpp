#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiExporter.h"

SynthesiaAIEditor::SynthesiaAIEditor(SynthesiaAIProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(600, 500);
    
    // Title
    titleLabel.setText("Synthesia AI - MIDI Generator", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);
    
    // Key selection
    keyLabel.setText("Key:", juce::dontSendNotification);
    addAndMakeVisible(keyLabel);
    
    keyComboBox.addItem("C", 1);
    keyComboBox.addItem("C#", 2);
    keyComboBox.addItem("D", 3);
    keyComboBox.addItem("D#", 4);
    keyComboBox.addItem("E", 5);
    keyComboBox.addItem("F", 6);
    keyComboBox.addItem("F#", 7);
    keyComboBox.addItem("G", 8);
    keyComboBox.addItem("G#", 9);
    keyComboBox.addItem("A", 10);
    keyComboBox.addItem("A#", 11);
    keyComboBox.addItem("B", 12);
    keyComboBox.setSelectedId(1);
    addAndMakeVisible(keyComboBox);
    
    // Scale selection
    scaleLabel.setText("Scale:", juce::dontSendNotification);
    addAndMakeVisible(scaleLabel);
    
    scaleComboBox.addItem("Major", 1);
    scaleComboBox.addItem("Minor", 2);
    scaleComboBox.addItem("Dorian", 3);
    scaleComboBox.addItem("Mixolydian", 4);
    scaleComboBox.addItem("Pentatonic", 5);
    scaleComboBox.setSelectedId(1);
    addAndMakeVisible(scaleComboBox);
    
    // Length slider
    lengthLabel.setText("Length (bars):", juce::dontSendNotification);
    addAndMakeVisible(lengthLabel);
    
    lengthSlider.setRange(1, 16, 1);
    lengthSlider.setValue(4);
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible(lengthSlider);
    
    // Tempo sync
    tempoSyncLabel.setText("Tempo Sync:", juce::dontSendNotification);
    addAndMakeVisible(tempoSyncLabel);
    
    tempoSyncComboBox.addItem("1/16", 1);
    tempoSyncComboBox.addItem("1/8", 2);
    tempoSyncComboBox.addItem("1/4", 3);
    tempoSyncComboBox.addItem("1/2", 4);
    tempoSyncComboBox.setSelectedId(2);
    addAndMakeVisible(tempoSyncComboBox);
    
    // Style/prompt
    styleLabel.setText("Style/Prompt:", juce::dontSendNotification);
    addAndMakeVisible(styleLabel);
    
    styleTextEditor.setMultiLine(true);
    styleTextEditor.setReturnKeyStartsNewLine(true);
    styleTextEditor.setText("jazzy chords with walking bass");
    addAndMakeVisible(styleTextEditor);
    
    // Generator selection
    generatorLabel.setText("Generator:", juce::dontSendNotification);
    addAndMakeVisible(generatorLabel);
    
    generatorComboBox.addItem("Markov (Offline)", 1);
    generatorComboBox.addItem("LLM (OpenAI API)", 2);
    generatorComboBox.setSelectedId(
        audioProcessor.getCurrentGeneratorType() == GeneratorType::Markov ? 1 : 2);
    generatorComboBox.onChange = [this]()
    {
        audioProcessor.setGeneratorType(
            generatorComboBox.getSelectedId() == 1 
                ? GeneratorType::Markov 
                : GeneratorType::LLM);
    };
    addAndMakeVisible(generatorComboBox);
    
    // Generate button
    generateButton.setButtonText("Generate MIDI");
    generateButton.onClick = [this]() { onGenerateClicked(); };
    addAndMakeVisible(generateButton);
    
    // Export button
    exportButton.setButtonText("Export as .mid");
    exportButton.setEnabled(false);
    exportButton.onClick = [this]() { onExportMidi(); };
    addAndMakeVisible(exportButton);
    
    // Status label
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);
    
    // Settings button
    settingsButton.setButtonText("Settings");
    settingsButton.onClick = [this]()
    {
        auto* dialog = new SettingsDialog(audioProcessor);
        dialog->setSize(400, 200);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialog);
        options.dialogTitle = "Settings";
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.dialogBackgroundColour = getLookAndFeel().findColour(
            juce::ResizableWindow::backgroundColourId);
        options.launchAsync();
    };
    addAndMakeVisible(settingsButton);
    
    startTimer(100);
}

SynthesiaAIEditor::~SynthesiaAIEditor()
{
}

void SynthesiaAIEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SynthesiaAIEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    titleLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);
    
    auto row = area.removeFromTop(30);
    keyLabel.setBounds(row.removeFromLeft(100));
    keyComboBox.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(20);
    scaleLabel.setBounds(row.removeFromLeft(100));
    scaleComboBox.setBounds(row);
    
    area.removeFromTop(10);
    row = area.removeFromTop(30);
    lengthLabel.setBounds(row.removeFromLeft(120));
    lengthSlider.setBounds(row.removeFromLeft(150));
    row.removeFromLeft(20);
    tempoSyncLabel.setBounds(row.removeFromLeft(100));
    tempoSyncComboBox.setBounds(row);
    
    area.removeFromTop(10);
    styleLabel.setBounds(area.removeFromTop(25));
    styleTextEditor.setBounds(area.removeFromTop(80));
    
    area.removeFromTop(10);
    row = area.removeFromTop(30);
    generatorLabel.setBounds(row.removeFromLeft(100));
    generatorComboBox.setBounds(row.removeFromLeft(200));
    
    area.removeFromTop(20);
    generateButton.setBounds(area.removeFromTop(40));
    
    area.removeFromTop(10);
    exportButton.setBounds(area.removeFromTop(40));
    
    area.removeFromTop(10);
    statusLabel.setBounds(area.removeFromTop(30));
    
    settingsButton.setBounds(getWidth() - 100, 10, 80, 30);
}

void SynthesiaAIEditor::timerCallback()
{
    updateUIState();
}

void SynthesiaAIEditor::onGenerateClicked()
{
    MidiGenerationParams params;
    params.rootNote = keyComboBox.getSelectedId() - 1;
    params.scale = static_cast<ScaleType>(scaleComboBox.getSelectedId() - 1);
    params.lengthInBars = static_cast<int>(lengthSlider.getValue());
    params.quantization = static_cast<Quantization>(tempoSyncComboBox.getSelectedId() - 1);
    params.stylePrompt = styleTextEditor.getText().toStdString();
    params.tempo = 120.0; // Default tempo
    
    audioProcessor.generateMidi(params);
    statusLabel.setText("Generating...", juce::dontSendNotification);
}

void SynthesiaAIEditor::onExportMidi()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save MIDI file...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.mid");
    
    auto flags = juce::FileBrowserComponent::saveMode 
               | juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            if (!file.hasFileExtension(".mid"))
                file = file.withFileExtension(".mid");
            
            if (MidiExporter::exportToFile(audioProcessor.getGeneratedMidi(), file))
                statusLabel.setText("Exported successfully!", juce::dontSendNotification);
            else
                statusLabel.setText("Export failed!", juce::dontSendNotification);
        }
    });
}

void SynthesiaAIEditor::updateUIState()
{
    bool isGenerating = audioProcessor.isGenerating();
    generateButton.setEnabled(!isGenerating);
    
    bool hasMidi = !audioProcessor.getGeneratedMidi().isEmpty();
    exportButton.setEnabled(hasMidi && !isGenerating);
    
    if (!isGenerating && hasMidi && statusLabel.getText() == "Generating...")
        statusLabel.setText("Generation complete!", juce::dontSendNotification);
}

// SettingsDialog implementation
SettingsDialog::SettingsDialog(SynthesiaAIProcessor& p)
    : processor(p)
{
    apiKeyLabel.setText("OpenAI API Key:", juce::dontSendNotification);
    addAndMakeVisible(apiKeyLabel);
    
    apiKeyEditor.setText(processor.getApiKey());
    apiKeyEditor.setPasswordCharacter('*');
    addAndMakeVisible(apiKeyEditor);
    
    saveButton.setButtonText("Save");
    saveButton.onClick = [this]()
    {
        processor.setApiKey(apiKeyEditor.getText());
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };
    addAndMakeVisible(saveButton);
    
    cancelButton.setButtonText("Cancel");
    cancelButton.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(cancelButton);
}

void SettingsDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void SettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    apiKeyLabel.setBounds(area.removeFromTop(25));
    area.removeFromTop(5);
    apiKeyEditor.setBounds(area.removeFromTop(30));
    
    area.removeFromTop(20);
    auto buttonRow = area.removeFromTop(30);
    saveButton.setBounds(buttonRow.removeFromLeft(100));
    buttonRow.removeFromLeft(10);
    cancelButton.setBounds(buttonRow.removeFromLeft(100));
}
