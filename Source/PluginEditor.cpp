#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiExporter.h"
#include <map>

namespace Theme
{
    const juce::Colour background{0xff1b1f2a};
    const juce::Colour header{0xff232838};
    const juce::Colour outline{0xff2e3445};
    const juce::Colour clipBackground{0xff141822};
    const juce::Colour accent{0xff4f8cff};
    const juce::Colour label{0xffb8c0d0};
    const juce::Colour dimText{0xff7a8396};
}

namespace
{
    constexpr int labelWidth = 92;

    void setUpLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, Theme::label);
    }
}

// ClipView implementation
void ClipView::setClip(const juce::MidiBuffer& midi, double lengthInBeats)
{
    notes.clear();
    clipLength = lengthInBeats;

    std::map<int, std::pair<double, int>> openNotes;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const double beat = metadata.samplePosition / 960.0;

        if (message.isNoteOn())
        {
            openNotes[message.getNoteNumber()] = { beat, message.getVelocity() };
        }
        else if (message.isNoteOff())
        {
            auto it = openNotes.find(message.getNoteNumber());
            if (it != openNotes.end())
            {
                notes.push_back({ it->first, it->second.first, beat - it->second.first, it->second.second });
                openNotes.erase(it);
            }
        }
    }

    lowestPitch = 127;
    highestPitch = 0;
    for (const auto& note : notes)
    {
        lowestPitch = juce::jmin(lowestPitch, note.pitch);
        highestPitch = juce::jmax(highestPitch, note.pitch);
    }

    if (notes.empty())
    {
        lowestPitch = 60;
        highestPitch = 72;
    }

    repaint();
}

void ClipView::setPlaybackBeat(double beat)
{
    if (std::abs(beat - playbackBeat) > 1.0e-4)
    {
        playbackBeat = beat;
        repaint();
    }
}

void ClipView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(Theme::clipBackground);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(Theme::outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

    if (clipLength <= 0.0 || notes.empty())
    {
        g.setColour(Theme::dimText);
        g.setFont(14.0f);
        g.drawText("Generate a clip to see it here", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto area = bounds.reduced(10.0f, 8.0f);
    auto caption = area.removeFromTop(16.0f);
    area.removeFromTop(4.0f);

    g.setColour(Theme::dimText);
    g.setFont(12.0f);
    g.drawText(juce::String(juce::roundToInt(clipLength / 4.0)) + " bars  |  "
                   + juce::String(static_cast<int>(notes.size())) + " notes  |  "
                   + juce::MidiMessage::getMidiNoteName(lowestPitch, true, true, 4) + " - "
                   + juce::MidiMessage::getMidiNoteName(highestPitch, true, true, 4),
               caption, juce::Justification::centredLeft);

    const float pixelsPerBeat = area.getWidth() / static_cast<float>(clipLength);
    const int pitchRows = highestPitch - lowestPitch + 3;
    const float rowHeight = area.getHeight() / static_cast<float>(pitchRows);

    for (int beat = 0; beat <= static_cast<int>(clipLength); ++beat)
    {
        const float x = area.getX() + beat * pixelsPerBeat;
        g.setColour(juce::Colours::white.withAlpha(beat % 4 == 0 ? 0.18f : 0.05f));
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
    }

    for (const auto& note : notes)
    {
        const float x = area.getX() + static_cast<float>(note.start) * pixelsPerBeat;
        const float w = juce::jmax(2.0f, static_cast<float>(note.length) * pixelsPerBeat - 1.0f);
        const float y = area.getY() + (highestPitch + 1 - note.pitch) * rowHeight;
        const float alpha = juce::jmap(static_cast<float>(note.velocity), 0.0f, 127.0f, 0.45f, 1.0f);

        g.setColour(Theme::accent.withAlpha(alpha));
        g.fillRoundedRectangle(x, y, w, juce::jmax(2.0f, rowHeight - 1.0f), 2.0f);
    }

    if (playbackBeat >= 0.0)
    {
        const float x = area.getX() + static_cast<float>(playbackBeat) * pixelsPerBeat;
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(x, area.getY(), 1.5f, area.getHeight());
    }
}

SynthesiaAIEditor::SynthesiaAIEditor(SynthesiaAIProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    titleLabel.setText("Synthesia AI", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("AI MIDI generator", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(subtitleLabel);
    
    setUpLabel(keyLabel, "Key");
    addAndMakeVisible(keyLabel);
    
    const char* keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        keyComboBox.addItem(keyNames[i], i + 1);
    keyComboBox.setSelectedId(1);
    addAndMakeVisible(keyComboBox);
    
    setUpLabel(scaleLabel, "Scale");
    addAndMakeVisible(scaleLabel);
    
    scaleComboBox.addItem("Major", 1);
    scaleComboBox.addItem("Minor", 2);
    scaleComboBox.addItem("Dorian", 3);
    scaleComboBox.addItem("Mixolydian", 4);
    scaleComboBox.addItem("Pentatonic", 5);
    scaleComboBox.setSelectedId(1);
    addAndMakeVisible(scaleComboBox);
    
    setUpLabel(lengthLabel, "Length (bars)");
    addAndMakeVisible(lengthLabel);
    
    lengthSlider.setRange(1, 16, 1);
    lengthSlider.setValue(4);
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 24);
    lengthSlider.setColour(juce::Slider::thumbColourId, Theme::accent);
    lengthSlider.setColour(juce::Slider::trackColourId, Theme::accent.withAlpha(0.6f));
    addAndMakeVisible(lengthSlider);
    
    setUpLabel(tempoSyncLabel, "Tempo sync");
    addAndMakeVisible(tempoSyncLabel);
    
    tempoSyncComboBox.addItem("1/16", 1);
    tempoSyncComboBox.addItem("1/8", 2);
    tempoSyncComboBox.addItem("1/4", 3);
    tempoSyncComboBox.addItem("1/2", 4);
    tempoSyncComboBox.setSelectedId(2);
    addAndMakeVisible(tempoSyncComboBox);
    
    setUpLabel(styleLabel, "Style / prompt");
    addAndMakeVisible(styleLabel);
    
    styleTextEditor.setMultiLine(true);
    styleTextEditor.setReturnKeyStartsNewLine(true);
    styleTextEditor.setText("jazzy chords with walking bass");
    styleTextEditor.setTextToShowWhenEmpty("Describe the style (used by the LLM generator)", Theme::dimText);
    styleTextEditor.setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground);
    styleTextEditor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
    styleTextEditor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
    addAndMakeVisible(styleTextEditor);
    
    setUpLabel(generatorLabel, "Generator");
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
    
    generateButton.setButtonText("Generate MIDI");
    generateButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    generateButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    generateButton.onClick = [this]() { onGenerateClicked(); };
    addAndMakeVisible(generateButton);

    addAndMakeVisible(clipView);

    previewButton.setButtonText("Play Preview");
    previewButton.setEnabled(false);
    previewButton.onClick = [this]()
    {
        audioProcessor.setPreviewPlaying(!audioProcessor.isPreviewPlaying());
    };
    addAndMakeVisible(previewButton);
    
    exportButton.setButtonText("Export as .mid");
    exportButton.setEnabled(false);
    exportButton.onClick = [this]() { onExportMidi(); };
    addAndMakeVisible(exportButton);
    
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(statusLabel);
    
    settingsButton.setButtonText("Settings");
    settingsButton.onClick = [this]()
    {
        auto* dialog = new SettingsDialog(audioProcessor);
        dialog->setSize(440, 180);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialog);
        options.dialogTitle = "Synthesia AI Settings";
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.dialogBackgroundColour = Theme::background;
        options.launchAsync();
    };
    addAndMakeVisible(settingsButton);

    setSize(620, 590);
    startTimerHz(30);
}

SynthesiaAIEditor::~SynthesiaAIEditor()
{
}

void SynthesiaAIEditor::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background);

    auto header = getLocalBounds().removeFromTop(64);
    g.setColour(Theme::header);
    g.fillRect(header);
    g.setColour(Theme::outline);
    g.drawHorizontalLine(header.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));
}

void SynthesiaAIEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop(64).reduced(20, 8);
    settingsButton.setBounds(header.removeFromRight(96).withSizeKeepingCentre(96, 30));
    titleLabel.setBounds(header.removeFromTop(28));
    subtitleLabel.setBounds(header);

    area = area.reduced(20, 0);
    area.removeFromTop(16);

    auto layoutCell = [](juce::Rectangle<int> cell, juce::Label& label, juce::Component& control)
    {
        label.setBounds(cell.removeFromLeft(labelWidth));
        control.setBounds(cell);
    };

    const int columnGap = 24;
    auto twoColumnRow = [&](juce::Label& leftLabel, juce::Component& left,
                            juce::Label& rightLabel, juce::Component& right)
    {
        auto row = area.removeFromTop(28);
        auto leftCell = row.removeFromLeft((row.getWidth() - columnGap) / 2);
        row.removeFromLeft(columnGap);
        layoutCell(leftCell, leftLabel, left);
        layoutCell(row, rightLabel, right);
        area.removeFromTop(10);
    };

    twoColumnRow(keyLabel, keyComboBox, scaleLabel, scaleComboBox);
    twoColumnRow(lengthLabel, lengthSlider, tempoSyncLabel, tempoSyncComboBox);

    auto row = area.removeFromTop(28);
    layoutCell(row.removeFromLeft((row.getWidth() - columnGap) / 2), generatorLabel, generatorComboBox);
    area.removeFromTop(8);

    styleLabel.setBounds(area.removeFromTop(22));
    styleTextEditor.setBounds(area.removeFromTop(56));
    area.removeFromTop(14);

    generateButton.setBounds(area.removeFromTop(40));
    area.removeFromTop(14);

    clipView.setBounds(area.removeFromTop(150));
    area.removeFromTop(12);

    row = area.removeFromTop(34);
    previewButton.setBounds(row.removeFromLeft((row.getWidth() - 10) / 2));
    row.removeFromLeft(10);
    exportButton.setBounds(row);
    area.removeFromTop(8);

    statusLabel.setBounds(area.removeFromTop(22));
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
    
    auto chooserFlags = juce::FileBrowserComponent::saveMode 
                      | juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            if (!file.hasFileExtension(".mid"))
                file = file.withFileExtension(".mid");
            
            if (MidiExporter::exportToFile(audioProcessor.getGeneratedMidi(), file))
                statusLabel.setText("Exported " + file.getFileName(), juce::dontSendNotification);
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
    previewButton.setEnabled(hasMidi && !isGenerating);
    previewButton.setButtonText(audioProcessor.isPreviewPlaying() ? "Stop Preview" : "Play Preview");

    if (shownClipVersion != audioProcessor.getClipVersion())
    {
        shownClipVersion = audioProcessor.getClipVersion();
        clipView.setClip(audioProcessor.getGeneratedMidi(), audioProcessor.getClipLengthInBeats());
    }
    clipView.setPlaybackBeat(audioProcessor.getPlaybackBeat());
    
    if (!isGenerating && statusLabel.getText() == "Generating...")
        statusLabel.setText(hasMidi ? "Generation complete!"
                                    : "Generation failed (check generator settings)",
                            juce::dontSendNotification);
}

// SettingsDialog implementation
SettingsDialog::SettingsDialog(SynthesiaAIProcessor& p)
    : processor(p)
{
    setUpLabel(apiKeyLabel, "OpenAI API key");
    addAndMakeVisible(apiKeyLabel);
    
    apiKeyEditor.setText(processor.getApiKey());
    apiKeyEditor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    apiKeyEditor.setTextToShowWhenEmpty("sk-...", Theme::dimText);
    apiKeyEditor.setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground);
    apiKeyEditor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
    apiKeyEditor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
    addAndMakeVisible(apiKeyEditor);

    hintLabel.setText("Used by the LLM generator. The key is saved with your project, "
                      "so don't share project files that contain it.",
                      juce::dontSendNotification);
    hintLabel.setFont(juce::Font(12.5f));
    hintLabel.setColour(juce::Label::textColourId, Theme::dimText);
    hintLabel.setJustificationType(juce::Justification::topLeft);
    hintLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(hintLabel);
    
    saveButton.setButtonText("Save");
    saveButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
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
    g.fillAll(Theme::background);
}

void SettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    apiKeyLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(4);
    apiKeyEditor.setBounds(area.removeFromTop(30));
    area.removeFromTop(8);
    hintLabel.setBounds(area.removeFromTop(36));

    auto buttonRow = area.removeFromBottom(32);
    saveButton.setBounds(buttonRow.removeFromRight(96));
    buttonRow.removeFromRight(10);
    cancelButton.setBounds(buttonRow.removeFromRight(96));
}
