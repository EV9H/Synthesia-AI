#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiExporter.h"
#include "Generators/GenreProfiles.h"
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

    juce::Colour colourForChannel(int channel)
    {
        switch (channel)
        {
            case 1: return juce::Colour(0xffe8b86d); // chords
            case 2: return juce::Colour(0xfff07178); // bass
            case 3: return juce::Colour(0xff4f8cff); // synth
            case 4: return juce::Colour(0xff7dcea0); // piano
            default: return Theme::accent;
        }
    }

    void setUpLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, Theme::label);
    }

    int generatorComboId(GeneratorType type)
    {
        switch (type)
        {
            case GeneratorType::Genre:  return 1;
            case GeneratorType::Markov: return 2;
            case GeneratorType::LLM:    return 3;
        }
        return 1;
    }

    GeneratorType generatorFromComboId(int id)
    {
        switch (id)
        {
            case 2: return GeneratorType::Markov;
            case 3: return GeneratorType::LLM;
            default: return GeneratorType::Genre;
        }
    }
}

void ClipView::setClip(const juce::MidiBuffer& midi, double lengthInBeats,
                       const std::vector<ChordInfo>& chords)
{
    notes.clear();
    progression = chords;
    clipLength = lengthInBeats;

    std::map<std::pair<int, int>, std::pair<double, int>> openNotes;

    auto closeNote = [this](int channel, int pitch, double start, int velocity, double end)
    {
        notes.push_back({ pitch, start, juce::jmax(0.0, end - start), velocity, channel });
    };

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const double beat = metadata.samplePosition / 960.0;
        const auto key = std::make_pair(message.getChannel(), message.getNoteNumber());

        if (message.isNoteOn())
        {
            auto it = openNotes.find(key);
            if (it != openNotes.end())
            {
                closeNote(key.first, key.second, it->second.first, it->second.second, beat);
                openNotes.erase(it);
            }
            openNotes[key] = { beat, message.getVelocity() };
        }
        else if (message.isNoteOff())
        {
            auto it = openNotes.find(key);
            if (it != openNotes.end())
            {
                closeNote(key.first, key.second, it->second.first, it->second.second, beat);
                openNotes.erase(it);
            }
        }
    }

    for (const auto& [key, start] : openNotes)
        closeNote(key.first, key.second, start.first, start.second, clipLength);

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
    area.removeFromTop(2.0f);
    auto legend = area.removeFromTop(16.0f);
    area.removeFromTop(2.0f);

    const bool drawChords = !progression.empty();
    auto chordStrip = drawChords ? area.removeFromTop(16.0f) : juce::Rectangle<float>{};
    if (drawChords)
        area.removeFromTop(2.0f);

    g.setColour(Theme::dimText);
    g.setFont(12.0f);
    g.drawText(juce::String(juce::roundToInt(clipLength / 4.0)) + " bars  |  "
                   + juce::String(static_cast<int>(notes.size())) + " notes  |  "
                   + juce::MidiMessage::getMidiNoteName(lowestPitch, true, true, 4) + " - "
                   + juce::MidiMessage::getMidiNoteName(highestPitch, true, true, 4),
               caption, juce::Justification::centredLeft);

    float legendX = legend.getX();
    g.setFont(11.0f);
    for (Part part : allParts)
    {
        const auto colour = colourForChannel(partChannel(part));
        g.setColour(colour);
        g.fillRoundedRectangle(legendX, legend.getY() + 3.0f, 10.0f, 10.0f, 2.0f);
        g.setColour(Theme::dimText);
        const auto name = juce::String(partName(part));
        g.drawText(name, juce::roundToInt(legendX + 14.0f), juce::roundToInt(legend.getY()),
                   52, juce::roundToInt(legend.getHeight()), juce::Justification::centredLeft, false);
        legendX += 68.0f;
    }

    const float pixelsPerBeat = area.getWidth() / static_cast<float>(clipLength);
    const int pitchRows = highestPitch - lowestPitch + 3;
    const float rowHeight = area.getHeight() / static_cast<float>(pitchRows);

    for (int beat = 0; beat <= static_cast<int>(clipLength); ++beat)
    {
        const float x = area.getX() + beat * pixelsPerBeat;
        const bool bar = beat % 4 == 0;
        g.setColour(juce::Colours::white.withAlpha(bar ? 0.18f : 0.05f));
        g.drawVerticalLine(juce::roundToInt(x), area.getY(), area.getBottom());
        if (drawChords)
        {
            g.setColour(juce::Colours::white.withAlpha(bar ? 0.18f : 0.05f));
            g.drawVerticalLine(juce::roundToInt(x), chordStrip.getY(), chordStrip.getBottom());
        }
    }

    if (drawChords)
    {
        g.setFont(11.0f);
        for (const auto& chord : progression)
        {
            const float x = chordStrip.getX() + static_cast<float>(chord.startBeat) * pixelsPerBeat;
            const float w = juce::jmax(24.0f, static_cast<float>(chord.lengthBeats) * pixelsPerBeat - 4.0f);
            g.setColour(Theme::label);
            g.drawText(chord.symbol,
                       juce::roundToInt(x + 2.0f), juce::roundToInt(chordStrip.getY()),
                       juce::roundToInt(w), juce::roundToInt(chordStrip.getHeight()),
                       juce::Justification::centredLeft, false);
        }
    }

    for (const auto& note : notes)
    {
        const float x = area.getX() + static_cast<float>(note.start) * pixelsPerBeat;
        const float w = juce::jmax(2.0f, static_cast<float>(note.length) * pixelsPerBeat - 1.0f);
        const float y = area.getY() + (highestPitch + 1 - note.pitch) * rowHeight;
        const float alpha = juce::jmap(static_cast<float>(note.velocity), 0.0f, 127.0f, 0.45f, 1.0f);

        g.setColour(colourForChannel(note.channel).withAlpha(alpha));
        g.fillRoundedRectangle(x, y, w, juce::jmax(2.0f, rowHeight - 1.0f), 2.0f);
    }

    if (playbackBeat >= 0.0)
    {
        const float x = area.getX() + static_cast<float>(playbackBeat) * pixelsPerBeat;
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.fillRect(x, area.getY(), 1.5f, area.getHeight());
        if (drawChords)
            g.fillRect(x, chordStrip.getY(), 1.5f, chordStrip.getHeight());
    }
}

SynthesiaAIEditor::SynthesiaAIEditor(SynthesiaAIProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    titleLabel.setText("Synthesia AI", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Genre-aware MIDI generator", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(subtitleLabel);
    
    setUpLabel(keyLabel, "Key");
    addAndMakeVisible(keyLabel);
    
    const char* keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        keyComboBox.addItem(keyNames[i], i + 1);
    keyComboBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(keyComboBox);
    
    setUpLabel(scaleLabel, "Scale");
    addAndMakeVisible(scaleLabel);
    
    scaleComboBox.addItem("Major", 1);
    scaleComboBox.addItem("Minor", 2);
    scaleComboBox.addItem("Dorian", 3);
    scaleComboBox.addItem("Mixolydian", 4);
    scaleComboBox.addItem("Pentatonic", 5);
    scaleComboBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(scaleComboBox);

    setUpLabel(genreLabel, "Genre");
    addAndMakeVisible(genreLabel);

    for (int i = 0; i < numGenres; ++i)
        genreComboBox.addItem(genreName(static_cast<Genre>(i)), i + 1);
    genreComboBox.setSelectedId(1, juce::dontSendNotification);
    genreComboBox.onChange = [this]() { applyGenreDefaults(); };
    addAndMakeVisible(genreComboBox);

    setUpLabel(partLabel, "Part");
    addAndMakeVisible(partLabel);

    partComboBox.addItem(partName(Part::All), 1);
    partComboBox.addItem(partName(Part::Chords), 2);
    partComboBox.addItem(partName(Part::Bass), 3);
    partComboBox.addItem(partName(Part::Synth), 4);
    partComboBox.addItem(partName(Part::Piano), 5);
    partComboBox.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(partComboBox);
    
    setUpLabel(lengthLabel, "Length (bars)");
    addAndMakeVisible(lengthLabel);
    
    lengthSlider.setRange(1, 16, 1);
    lengthSlider.setValue(8, juce::dontSendNotification);
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 24);
    lengthSlider.setColour(juce::Slider::thumbColourId, Theme::accent);
    lengthSlider.setColour(juce::Slider::trackColourId, Theme::accent.withAlpha(0.6f));
    addAndMakeVisible(lengthSlider);

    setUpLabel(tempoLabel, "Tempo");
    addAndMakeVisible(tempoLabel);

    tempoSlider.setRange(60, 180, 1);
    tempoSlider.setTextValueSuffix(" bpm");
    tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
    tempoSlider.setColour(juce::Slider::thumbColourId, Theme::accent);
    tempoSlider.setColour(juce::Slider::trackColourId, Theme::accent.withAlpha(0.6f));
    addAndMakeVisible(tempoSlider);
    
    setUpLabel(tempoSyncLabel, "Grid");
    addAndMakeVisible(tempoSyncLabel);
    
    tempoSyncComboBox.addItem("1/16", 1);
    tempoSyncComboBox.addItem("1/8", 2);
    tempoSyncComboBox.addItem("1/4", 3);
    tempoSyncComboBox.addItem("1/2", 4);
    addAndMakeVisible(tempoSyncComboBox);
    
    setUpLabel(styleLabel, "Style / prompt");
    addAndMakeVisible(styleLabel);
    
    styleTextEditor.setMultiLine(true);
    styleTextEditor.setReturnKeyStartsNewLine(true);
    styleTextEditor.setTextToShowWhenEmpty("Describe the style (used by the LLM generator)", Theme::dimText);
    styleTextEditor.setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground);
    styleTextEditor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
    styleTextEditor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
    addAndMakeVisible(styleTextEditor);
    
    setUpLabel(generatorLabel, "Generator");
    addAndMakeVisible(generatorLabel);
    
    generatorComboBox.addItem("Genre engine (offline)", 1);
    generatorComboBox.addItem("Markov (classic)", 2);
    generatorComboBox.addItem("LLM (OpenAI API)", 3);
    generatorComboBox.setSelectedId(generatorComboId(audioProcessor.getCurrentGeneratorType()),
                                    juce::dontSendNotification);
    generatorComboBox.onChange = [this]()
    {
        audioProcessor.setGeneratorType(generatorFromComboId(generatorComboBox.getSelectedId()));
    };
    addAndMakeVisible(generatorComboBox);

    applyGenreDefaults();
    
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
    
    exportButton.setButtonText("Export MIDI...");
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

    setSize(660, 700);
    startTimerHz(30);
}

SynthesiaAIEditor::~SynthesiaAIEditor()
{
}

void SynthesiaAIEditor::applyGenreDefaults()
{
    const int id = genreComboBox.getSelectedId();
    if (id <= 0)
        return;

    const auto& profile = getGenreProfile(static_cast<Genre>(id - 1));
    tempoSlider.setValue(profile.defaultTempo, juce::dontSendNotification);
    tempoSyncComboBox.setSelectedId(static_cast<int>(profile.defaultGrid) + 1,
                                    juce::dontSendNotification);
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
    twoColumnRow(genreLabel, genreComboBox, partLabel, partComboBox);
    twoColumnRow(lengthLabel, lengthSlider, tempoLabel, tempoSlider);
    twoColumnRow(tempoSyncLabel, tempoSyncComboBox, generatorLabel, generatorComboBox);

    styleLabel.setBounds(area.removeFromTop(22));
    styleTextEditor.setBounds(area.removeFromTop(48));
    area.removeFromTop(14);

    generateButton.setBounds(area.removeFromTop(40));
    area.removeFromTop(14);

    clipView.setBounds(area.removeFromTop(210));
    area.removeFromTop(12);

    auto row = area.removeFromTop(34);
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
    params.tempo = tempoSlider.getValue();
    params.genre = static_cast<Genre>(juce::jmax(0, genreComboBox.getSelectedId() - 1));
    params.part = static_cast<Part>(juce::jmax(0, partComboBox.getSelectedId() - 1));
    
    audioProcessor.generateMidi(params);
    statusLabel.setText("Generating...", juce::dontSendNotification);
}

void SynthesiaAIEditor::onExportMidi()
{
    const auto present = MidiExporter::partsIn(audioProcessor.getGeneratedMidi());

    juce::PopupMenu menu;
    menu.addItem(1, "Export multi-track .mid");
    menu.addItem(2, "Export separate files per part", present.size() > 1);
    if (!present.empty())
    {
        menu.addSeparator();
        for (auto part : present)
            menu.addItem(10 + static_cast<int>(part),
                         "Export " + juce::String(partName(part)) + " only");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),
                       [this](int result)
                       {
                           if (result != 0)
                               exportToChooser(result);
                       });
}

void SynthesiaAIEditor::exportToChooser(int menuId)
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save MIDI file...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.mid");
    
    auto chooserFlags = juce::FileBrowserComponent::saveMode 
                      | juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync(chooserFlags, [this, chooser, menuId](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File{})
            return;

        if (!file.hasFileExtension(".mid"))
            file = file.withFileExtension(".mid");

        const auto& midi = audioProcessor.getGeneratedMidi();
        const double tempo = audioProcessor.getClipTempo();
        bool ok = false;
        juce::String message;

        if (menuId == 1)
        {
            ok = MidiExporter::exportToFile(midi, file, tempo);
            message = "Exported " + file.getFileName();
        }
        else if (menuId == 2)
        {
            const auto written = MidiExporter::exportSeparateParts(midi, file, tempo);
            ok = !written.isEmpty();
            message = "Exported " + juce::String(written.size()) + " part files";
        }
        else
        {
            const auto part = static_cast<Part>(menuId - 10);
            ok = MidiExporter::exportToFile(MidiExporter::filterPart(midi, part), file, tempo);
            message = "Exported " + juce::String(partName(part)) + " to " + file.getFileName();
        }

        statusLabel.setText(ok ? message : "Export failed!", juce::dontSendNotification);
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
        clipView.setClip(audioProcessor.getGeneratedMidi(),
                         audioProcessor.getClipLengthInBeats(),
                         audioProcessor.getProgression());
    }
    clipView.setPlaybackBeat(audioProcessor.getPlaybackBeat());
    
    if (!isGenerating && statusLabel.getText() == "Generating...")
        statusLabel.setText(hasMidi ? "Generation complete!"
                                    : "Generation failed (check generator settings)",
                            juce::dontSendNotification);
}

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
