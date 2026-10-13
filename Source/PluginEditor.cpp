#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiExporter.h"
#include "Generators/GenreProfiles.h"
#include "PresetStore.h"
#include "LicenseStub.h"
#include <map>

namespace
{
    constexpr int labelWidth = 88;

    juce::Colour colourForChannel(int channel)
    {
        switch (channel)
        {
            case 1:  return juce::Colour(0xffe8b86d);
            case 2:  return juce::Colour(0xfff07178);
            case 3:  return juce::Colour(0xff5b8def);
            case 4:  return juce::Colour(0xff7dcea0);
            case 10: return juce::Colour(0xffc4a5e8);
            default: return Theme::accent;
        }
    }

    void setUpLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, Theme::label);
        label.setFont(juce::Font(14.5f));
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

    bool partAudible(int muteMask, int soloMask, int channel)
    {
        const auto part = partFromChannel(channel);
        if (soloMask != 0)
            return (soloMask & partLockBit(part)) != 0;
        return (muteMask & partLockBit(part)) == 0;
    }
}

SynthesiaLookAndFeel::SynthesiaLookAndFeel()
{
    setColourScheme({
        0xff12151c, 0xff1c2230, 0xff1a1f2b, 0xff2c3448,
        0xffd5dbe8, 0xff5b8def, 0xffffffff, 0xff5b8def, 0xffffffff
    });
    setColour(juce::ResizableWindow::backgroundColourId, Theme::background);
    setColour(juce::TextButton::buttonColourId, Theme::panel);
    setColour(juce::TextButton::textColourOffId, Theme::label);
    setColour(juce::ComboBox::backgroundColourId, Theme::clipBackground);
    setColour(juce::ComboBox::textColourId, Theme::label);
    setColour(juce::ComboBox::outlineColourId, Theme::outline);
    setColour(juce::ComboBox::arrowColourId, Theme::dimText);
    setColour(juce::PopupMenu::backgroundColourId, Theme::header);
    setColour(juce::PopupMenu::textColourId, Theme::label);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::TabbedComponent::backgroundColourId, Theme::background);
    setColour(juce::TabbedComponent::outlineColourId, Theme::outline);
    setColour(juce::TabbedButtonBar::tabOutlineColourId, Theme::outline);
    setColour(juce::TabbedButtonBar::frontOutlineColourId, Theme::accent);
    setColour(juce::Slider::thumbColourId, Theme::accent);
    setColour(juce::Slider::trackColourId, Theme::accent.withAlpha(0.65f));
    setColour(juce::Slider::backgroundColourId, Theme::clipBackground);
    setColour(juce::Slider::textBoxTextColourId, Theme::label);
    setColour(juce::Slider::textBoxBackgroundColourId, Theme::clipBackground);
    setColour(juce::Slider::textBoxOutlineColourId, Theme::outline);
    setColour(juce::ToggleButton::textColourId, Theme::label);
    setColour(juce::ToggleButton::tickColourId, Theme::accent);
    setColour(juce::Label::textColourId, Theme::label);
    setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground);
    setColour(juce::TextEditor::textColourId, Theme::label);
    setColour(juce::TextEditor::outlineColourId, Theme::outline);
    setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
    setColour(juce::TextEditor::highlightColourId, Theme::accent.withAlpha(0.35f));
    setColour(juce::ScrollBar::thumbColourId, Theme::outline);
}

juce::Font SynthesiaLookAndFeel::getTextButtonFont(juce::TextButton&, int)
{
    return juce::Font(15.0f);
}

juce::Font SynthesiaLookAndFeel::getLabelFont(juce::Label&)
{
    return juce::Font(14.5f);
}

juce::Font SynthesiaLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return juce::Font(15.0f);
}

juce::Font SynthesiaLookAndFeel::getPopupMenuFont()
{
    return juce::Font(15.0f);
}

MidiDragChip::MidiDragChip(std::function<void()> dragFn, juce::String text)
    : onDrag(std::move(dragFn)), label(std::move(text))
{
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

void MidiDragChip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(hasMidi ? Theme::panel : Theme::clipBackground);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(hasMidi ? Theme::accent.withAlpha(0.85f) : Theme::outline);
    g.drawRoundedRectangle(bounds, 6.0f, 1.2f);
    g.setColour(hasMidi ? Theme::label : Theme::dimText);
    g.setFont(juce::Font(13.5f, juce::Font::bold));
    g.drawText(label, getLocalBounds().reduced(6, 0), juce::Justification::centred, false);
}

void MidiDragChip::mouseDown(const juce::MouseEvent&)
{
    dragging = false;
}

void MidiDragChip::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging || !hasMidi || event.getDistanceFromDragStart() < 4)
        return;
    dragging = true;
    if (onDrag)
        onDrag();
}

void MidiDragChip::setEnabledLook(bool midiReady)
{
    hasMidi = midiReady;
    repaint();
}

void ClipView::setClip(const juce::MidiBuffer& midi, double lengthInBeats,
                       const std::vector<ChordInfo>& chords,
                       const std::vector<SectionInfo>& formSections)
{
    notes.clear();
    progression = chords;
    sections = formSections;
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
        if (!noteAudible(note.channel))
            continue;
        lowestPitch = juce::jmin(lowestPitch, note.pitch);
        highestPitch = juce::jmax(highestPitch, note.pitch);
    }

    if (lowestPitch > highestPitch)
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

void ClipView::setMuteSolo(int mute, int solo)
{
    if (mute == muteMask && solo == soloMask)
        return;
    muteMask = mute;
    soloMask = solo;
    lowestPitch = 127;
    highestPitch = 0;
    for (const auto& note : notes)
    {
        if (!noteAudible(note.channel))
            continue;
        lowestPitch = juce::jmin(lowestPitch, note.pitch);
        highestPitch = juce::jmax(highestPitch, note.pitch);
    }
    if (lowestPitch > highestPitch)
    {
        lowestPitch = 60;
        highestPitch = 72;
    }
    repaint();
}

bool ClipView::noteAudible(int channel) const
{
    return partAudible(muteMask, soloMask, channel);
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
        g.setFont(15.0f);
        g.drawText("Generate a clip, then drag a part out as .mid", getLocalBounds(),
                   juce::Justification::centred);
        return;
    }

    auto area = bounds.reduced(10.0f, 8.0f);
    auto caption = area.removeFromTop(16.0f);
    area.removeFromTop(2.0f);
    auto legend = area.removeFromTop(16.0f);
    area.removeFromTop(2.0f);

    const bool drawSections = !sections.empty() && sections.size() > 1;
    auto sectionStrip = drawSections ? area.removeFromTop(14.0f) : juce::Rectangle<float>{};
    if (drawSections)
        area.removeFromTop(2.0f);

    const bool drawChords = !progression.empty();
    auto chordStrip = drawChords ? area.removeFromTop(16.0f) : juce::Rectangle<float>{};
    if (drawChords)
        area.removeFromTop(2.0f);

    int audibleNotes = 0;
    for (const auto& note : notes)
        if (noteAudible(note.channel))
            ++audibleNotes;

    g.setColour(Theme::dimText);
    g.setFont(12.5f);
    g.drawText(juce::String(juce::roundToInt(clipLength / 4.0)) + " bars  |  "
                   + juce::String(audibleNotes) + " notes  |  "
                   + juce::MidiMessage::getMidiNoteName(lowestPitch, true, true, 4) + " - "
                   + juce::MidiMessage::getMidiNoteName(highestPitch, true, true, 4),
               caption, juce::Justification::centredLeft);

    float legendX = legend.getX();
    g.setFont(11.5f);
    for (Part part : allParts)
    {
        const bool audible = partAudible(muteMask, soloMask, partChannel(part));
        const auto colour = colourForChannel(partChannel(part)).withAlpha(audible ? 1.0f : 0.28f);
        g.setColour(colour);
        g.fillRoundedRectangle(legendX, legend.getY() + 3.0f, 10.0f, 10.0f, 2.0f);
        g.setColour(audible ? Theme::dimText : Theme::dimText.withAlpha(0.45f));
        const auto name = juce::String(partName(part));
        g.drawText(name, juce::roundToInt(legendX + 14.0f), juce::roundToInt(legend.getY()),
                   56, juce::roundToInt(legend.getHeight()), juce::Justification::centredLeft, false);
        legendX += 68.0f;
    }

    const float pixelsPerBeat = area.getWidth() / static_cast<float>(clipLength);
    const int pitchRows = highestPitch - lowestPitch + 3;
    const float rowHeight = area.getHeight() / static_cast<float>(juce::jmax(1, pitchRows));

    if (drawSections)
    {
        static const juce::Colour sectionColours[] = {
            juce::Colour(0xff3d4a63), juce::Colour(0xff35556e), juce::Colour(0xff4a5a3d),
            juce::Colour(0xff6a4a3a), juce::Colour(0xff4a3d63), juce::Colour(0xff6a3d4a),
            juce::Colour(0xff3d3d3d), juce::Colour(0xff3d4a4a)
        };
        g.setFont(10.5f);
        for (const auto& section : sections)
        {
            const float x = sectionStrip.getX() + static_cast<float>(section.startBeat) * pixelsPerBeat;
            const float w = juce::jmax(8.0f, static_cast<float>(section.lengthBeats) * pixelsPerBeat - 2.0f);
            const auto colour = sectionColours[static_cast<int>(section.kind) % 8];
            g.setColour(colour);
            g.fillRoundedRectangle(x, sectionStrip.getY(), w, sectionStrip.getHeight(), 3.0f);
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawText(section.label, juce::roundToInt(x + 3.0f), juce::roundToInt(sectionStrip.getY()),
                       juce::roundToInt(w - 4.0f), juce::roundToInt(sectionStrip.getHeight()),
                       juce::Justification::centredLeft, false);
        }
    }

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
        g.setFont(12.0f);
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
        const bool audible = noteAudible(note.channel);
        const float x = area.getX() + static_cast<float>(note.start) * pixelsPerBeat;
        const float w = juce::jmax(2.0f, static_cast<float>(note.length) * pixelsPerBeat - 1.0f);
        const float y = area.getY() + (highestPitch + 1 - note.pitch) * rowHeight;
        const float alpha = juce::jmap(static_cast<float>(note.velocity), 0.0f, 127.0f, 0.45f, 1.0f)
                            * (audible ? 1.0f : 0.18f);

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
        if (drawSections)
            g.fillRect(x, sectionStrip.getY(), 1.5f, sectionStrip.getHeight());
    }
}

SynthesiaAIEditor::SynthesiaAIEditor(SynthesiaAIProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    titleLabel.setText("Synthesia AI", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Presets, piano roll, drag MIDI to your DAW",
                          juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setColour(juce::Label::textColourId, Theme::dimText);
    subtitleLabel.setMinimumHorizontalScale(0.75f);
    addAndMakeVisible(subtitleLabel);

    licenseBadge.setJustificationType(juce::Justification::centred);
    licenseBadge.setFont(juce::Font(12.5f, juce::Font::bold));
    licenseBadge.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(licenseBadge);
    updateLicenseBadge();

    setUpLabel(presetLabel, "Preset");
    addAndMakeVisible(presetLabel);
    presetComboBox.onChange = [this]() { applyPresetFromCombo(); };
    addAndMakeVisible(presetComboBox);

    savePresetButton.setButtonText("Save");
    savePresetButton.onClick = [this]() { onSavePresetClicked(); };
    addAndMakeVisible(savePresetButton);

    settingsButton.setButtonText("Settings");
    settingsButton.onClick = [this]()
    {
        auto* dialog = new SettingsDialog(audioProcessor);
        dialog->setSize(460, 520);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(dialog);
        options.dialogTitle = "Synthesia AI Settings";
        options.useNativeTitleBar = true;
        options.resizable = false;
        options.dialogBackgroundColour = Theme::background;
        options.launchAsync();
    };
    addAndMakeVisible(settingsButton);

    tabs.setOutline(0);
    tabs.setTabBarDepth(32);
    tabs.setColour(juce::TabbedComponent::backgroundColourId, Theme::background);
    tabs.addTab("Generate", Theme::header, &generatePage, false);
    tabs.addTab("Plan", Theme::header, &planPage, false);
    tabs.addTab("Import", Theme::header, &importPage, false);
    tabs.addTab("Style", Theme::header, &stylePage, false);
    addAndMakeVisible(tabs);

    setUpLabel(keyLabel, "Key");
    generatePage.addAndMakeVisible(keyLabel);
    const char* keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        keyComboBox.addItem(keyNames[i], i + 1);
    keyComboBox.setSelectedId(1, juce::dontSendNotification);
    generatePage.addAndMakeVisible(keyComboBox);

    setUpLabel(scaleLabel, "Scale");
    generatePage.addAndMakeVisible(scaleLabel);
    scaleComboBox.addItem("Major", 1);
    scaleComboBox.addItem("Minor", 2);
    scaleComboBox.addItem("Dorian", 3);
    scaleComboBox.addItem("Mixolydian", 4);
    scaleComboBox.addItem("Pentatonic", 5);
    scaleComboBox.setSelectedId(1, juce::dontSendNotification);
    generatePage.addAndMakeVisible(scaleComboBox);

    setUpLabel(genreLabel, "Genre");
    generatePage.addAndMakeVisible(genreLabel);
    for (int i = 0; i < numGenres; ++i)
        genreComboBox.addItem(genreName(static_cast<Genre>(i)), i + 1);
    genreComboBox.setSelectedId(1, juce::dontSendNotification);
    genreComboBox.onChange = [this]() { applyGenreDefaults(); };
    generatePage.addAndMakeVisible(genreComboBox);

    setUpLabel(partLabel, "Part");
    generatePage.addAndMakeVisible(partLabel);
    partComboBox.addItem(partName(Part::All), 1);
    for (auto part : allParts)
        partComboBox.addItem(partName(part), static_cast<int>(part) + 1);
    partComboBox.setSelectedId(1, juce::dontSendNotification);
    generatePage.addAndMakeVisible(partComboBox);

    setUpLabel(formLabel, "Form");
    generatePage.addAndMakeVisible(formLabel);
    formComboBox.addItem("Loop", 1);
    formComboBox.addItem("Song form", 2);
    formComboBox.setSelectedId(1, juce::dontSendNotification);
    formComboBox.onChange = [this]()
    {
        if (formComboBox.getSelectedId() == 2 && lengthSlider.getValue() < 16)
            lengthSlider.setValue(24, juce::dontSendNotification);
    };
    generatePage.addAndMakeVisible(formComboBox);

    setUpLabel(lengthLabel, "Length");
    generatePage.addAndMakeVisible(lengthLabel);
    lengthSlider.setRange(1, 32, 1);
    lengthSlider.setValue(8, juce::dontSendNotification);
    lengthSlider.setTextValueSuffix(" bars");
    lengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 24);
    styleSlider(lengthSlider);
    generatePage.addAndMakeVisible(lengthSlider);

    setUpLabel(tempoLabel, "Tempo");
    generatePage.addAndMakeVisible(tempoLabel);
    tempoSlider.setRange(60, 180, 1);
    tempoSlider.setTextValueSuffix(" bpm");
    tempoSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
    styleSlider(tempoSlider);
    generatePage.addAndMakeVisible(tempoSlider);

    setUpLabel(tempoSyncLabel, "Grid");
    generatePage.addAndMakeVisible(tempoSyncLabel);
    tempoSyncComboBox.addItem("1/16", 1);
    tempoSyncComboBox.addItem("1/8", 2);
    tempoSyncComboBox.addItem("1/4", 3);
    tempoSyncComboBox.addItem("1/2", 4);
    generatePage.addAndMakeVisible(tempoSyncComboBox);

    setUpLabel(generatorLabel, "Engine");
    generatePage.addAndMakeVisible(generatorLabel);
    generatorComboBox.addItem("Genre engine (offline)", 1);
    generatorComboBox.addItem("Markov (classic)", 2);
    generatorComboBox.addItem("LLM planner", 3);
    generatorComboBox.setSelectedId(generatorComboId(audioProcessor.getCurrentGeneratorType()),
                                    juce::dontSendNotification);
    generatorComboBox.onChange = [this]()
    {
        audioProcessor.setGeneratorType(generatorFromComboId(generatorComboBox.getSelectedId()));
        snapshotParams();
    };
    generatePage.addAndMakeVisible(generatorComboBox);

    setUpLabel(chordsLabel, "Chords");
    generatePage.addAndMakeVisible(chordsLabel);
    chordsEditor.setMultiLine(false);
    chordsEditor.setTextToShowWhenEmpty("Am9 Fmaj9 Dm7 E7b9   or   Am9 | Fmaj9 | Dm7 | E7b9", Theme::dimText);
    generatePage.addAndMakeVisible(chordsEditor);

    lockProgressionButton.setButtonText("Lock chords");
    generatePage.addAndMakeVisible(lockProgressionButton);

    setUpLabel(locksLabel, "Keep");
    generatePage.addAndMakeVisible(locksLabel);
    for (auto part : allParts)
    {
        auto* button = partLockButtons.add(new juce::ToggleButton(partName(part)));
        generatePage.addAndMakeVisible(button);
    }

    setUpLabel(styleLabel, "Style / prompt");
    generatePage.addAndMakeVisible(styleLabel);
    styleTextEditor.setMultiLine(true);
    styleTextEditor.setReturnKeyStartsNewLine(true);
    styleTextEditor.setTextToShowWhenEmpty("late-night synthwave, chorus should explode", Theme::dimText);
    generatePage.addAndMakeVisible(styleTextEditor);

    generateButton.setButtonText("Generate MIDI");
    generateButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    generateButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    generateButton.onClick = [this]() { onGenerateClicked(); };
    addAndMakeVisible(generateButton);

    setUpLabel(planLabel, "Arrangement plan");
    planPage.addAndMakeVisible(planLabel);
    planSummaryEditor.setMultiLine(true);
    planSummaryEditor.setReadOnly(true);
    planSummaryEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    planSummaryEditor.setTextToShowWhenEmpty(
        "Plan a track to see sections and chords here. Edit Chords on the Generate tab, then Generate.",
        Theme::dimText);
    planPage.addAndMakeVisible(planSummaryEditor);
    planButton.setButtonText("Plan");
    planButton.onClick = [this]() { onPlanClicked(); };
    planPage.addAndMakeVisible(planButton);

    setUpLabel(importModeLabel, "Mode");
    importPage.addAndMakeVisible(importModeLabel);
    importModeComboBox.addItem("Off", 1);
    importModeComboBox.addItem("Accompany", 2);
    importModeComboBox.addItem("Extend", 3);
    importModeComboBox.addItem("Rewrite", 4);
    importModeComboBox.addItem("Restyle", 5);
    importModeComboBox.setSelectedId(1, juce::dontSendNotification);
    importModeComboBox.onChange = [this]()
    {
        if (importModeComboBox.getSelectedId() == 3 && lengthSlider.getValue() < 16)
        {
            lengthSlider.setValue(24, juce::dontSendNotification);
            formComboBox.setSelectedId(2, juce::dontSendNotification);
        }
    };
    importPage.addAndMakeVisible(importModeComboBox);

    importButton.setButtonText("Import MIDI...");
    importButton.onClick = [this]() { onImportClicked(); };
    importPage.addAndMakeVisible(importButton);

    captureButton.setButtonText("Capture MIDI");
    captureButton.onClick = [this]() { onCaptureClicked(); };
    importPage.addAndMakeVisible(captureButton);

    importHintLabel.setText("Drop a .mid onto the plugin, import a file, or capture host MIDI. "
                            "Detected chords land on Generate (locked). Then pick Accompany, Extend, "
                            "Rewrite or Restyle and generate.",
                            juce::dontSendNotification);
    importHintLabel.setFont(juce::Font(14.5f));
    importHintLabel.setColour(juce::Label::textColourId, Theme::dimText);
    importHintLabel.setJustificationType(juce::Justification::topLeft);
    importPage.addAndMakeVisible(importHintLabel);

    setUpLabel(styleProfileLabel, "Profile");
    stylePage.addAndMakeVisible(styleProfileLabel);
    styleProfileComboBox.onChange = [this]()
    {
        if (styleProfileComboBox.getSelectedId() <= 1)
            audioProcessor.setCurrentStyle({});
        else
            audioProcessor.setCurrentStyle(styleProfileComboBox.getText());
        shownStyleVersion = audioProcessor.getStyleVersion();
        if (!audioProcessor.getCurrentStyle().empty())
            planSummaryEditor.setText(audioProcessor.getCurrentStyle().summary);
        if (styleProfileComboBox.getSelectedId() > 1 && blendSlider.getValue() < 25.0)
            blendSlider.setValue(100.0, juce::sendNotification);
        snapshotParams();
    };
    stylePage.addAndMakeVisible(styleProfileComboBox);

    setUpLabel(blendLabel, "Blend");
    stylePage.addAndMakeVisible(blendLabel);
    blendSlider.setRange(0, 100, 1);
    blendSlider.setValue(0, juce::dontSendNotification);
    blendSlider.setTextValueSuffix("%");
    blendSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 24);
    styleSlider(blendSlider);
    blendSlider.onValueChange = [this]()
    {
        audioProcessor.setStyleBlend(blendSlider.getValue() / 100.0);
        snapshotParams();
    };
    stylePage.addAndMakeVisible(blendSlider);

    learnStyleButton.setButtonText("Learn style...");
    learnStyleButton.onClick = [this]() { onLearnStyleClicked(); };
    stylePage.addAndMakeVisible(learnStyleButton);

    styleHintLabel.setText("Learn chord vocabulary, voicings, rhythm and swing from MIDI files "
                           "or a folder. Blend 0% is the genre alone; 100% follows the learned profile.",
                           juce::dontSendNotification);
    styleHintLabel.setFont(juce::Font(14.5f));
    styleHintLabel.setColour(juce::Label::textColourId, Theme::dimText);
    styleHintLabel.setJustificationType(juce::Justification::topLeft);
    stylePage.addAndMakeVisible(styleHintLabel);

    refreshStyleCombo();
    refreshPresetCombo();
    applyParamsToUI(audioProcessor.getLastUiParams());

    addAndMakeVisible(clipView);

    setUpLabel(mixLabel, "Mix");
    addAndMakeVisible(mixLabel);
    for (auto part : allParts)
    {
        auto* name = mixPartLabels.add(new juce::Label({}, partName(part)));
        name->setFont(juce::Font(12.0f));
        name->setColour(juce::Label::textColourId, Theme::dimText);
        addAndMakeVisible(name);

        auto* mute = muteButtons.add(new juce::TextButton("M"));
        mute->setClickingTogglesState(true);
        mute->setColour(juce::TextButton::buttonOnColourId, Theme::trial);
        mute->setTooltip(juce::String(partName(part)) + " mute");
        mute->onClick = [this, part, mute]()
        {
            audioProcessor.setPartMuted(part, mute->getToggleState());
            clipView.setMuteSolo(audioProcessor.getMuteMask(), audioProcessor.getSoloMask());
        };
        addAndMakeVisible(mute);

        auto* solo = soloButtons.add(new juce::TextButton("S"));
        solo->setClickingTogglesState(true);
        solo->setColour(juce::TextButton::buttonOnColourId, Theme::accentAlt);
        solo->setTooltip(juce::String(partName(part)) + " solo");
        solo->onClick = [this, part, solo]()
        {
            audioProcessor.setPartSoloed(part, solo->getToggleState());
            clipView.setMuteSolo(audioProcessor.getMuteMask(), audioProcessor.getSoloMask());
        };
        addAndMakeVisible(solo);
    }

    dragHintLabel.setText("Drag MIDI", juce::dontSendNotification);
    dragHintLabel.setFont(juce::Font(13.5f, juce::Font::bold));
    dragHintLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(dragHintLabel);

    auto addChip = [this](Part part)
    {
        auto* chip = dragChips.add(new MidiDragChip([this, part]() { startExternalMidiDrag(part); },
                                                    partName(part)));
        addAndMakeVisible(chip);
    };
    addChip(Part::All);
    for (auto part : allParts)
        addChip(part);

    previewButton.setButtonText("Play Preview");
    previewButton.setEnabled(false);
    previewButton.onClick = [this]()
    {
        audioProcessor.setPreviewPlaying(!audioProcessor.isPreviewPlaying());
    };
    addAndMakeVisible(previewButton);

    exportButton.setButtonText("Export MIDI");
    exportButton.setEnabled(false);
    exportButton.onClick = [this]() { onExportMidi(); };
    addAndMakeVisible(exportButton);

    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(statusLabel);

    setResizable(true, true);
    setResizeLimits(760, 620, 1400, 1100);
    setSize(920, 720);
    startTimerHz(30);
}

SynthesiaAIEditor::~SynthesiaAIEditor()
{
    snapshotParams();
    setLookAndFeel(nullptr);
}

void SynthesiaAIEditor::styleSlider(juce::Slider& slider)
{
    slider.setColour(juce::Slider::thumbColourId, Theme::accent);
    slider.setColour(juce::Slider::trackColourId, Theme::accent.withAlpha(0.6f));
}

void SynthesiaAIEditor::styleControl(juce::Component&)
{
}

void SynthesiaAIEditor::applyGenreDefaults()
{
    if (applyingPreset)
        return;
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

    auto header = getLocalBounds().removeFromTop(56);
    g.setColour(Theme::header);
    g.fillRect(header);
    g.setColour(Theme::outline);
    g.drawHorizontalLine(header.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));

    auto footer = getLocalBounds().removeFromBottom(250);
    g.setColour(Theme::panel.withAlpha(0.35f));
    g.fillRect(footer.removeFromTop(1).withHeight(1));

    if (draggingFiles)
    {
        g.setColour(Theme::accent.withAlpha(0.18f));
        g.fillRect(getLocalBounds());
        g.setColour(Theme::accent);
        g.drawRect(getLocalBounds().reduced(6), 2);
        g.setFont(18.0f);
        g.setColour(juce::Colours::white);
        g.drawText("Drop .mid files or a folder to import / learn a style",
                   getLocalBounds(), juce::Justification::centred);
    }
}

void SynthesiaAIEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop(56).reduced(16, 8);
    settingsButton.setBounds(header.removeFromRight(92).withSizeKeepingCentre(92, 30));
    header.removeFromRight(8);
    savePresetButton.setBounds(header.removeFromRight(64).withSizeKeepingCentre(64, 30));
    header.removeFromRight(8);
    presetComboBox.setBounds(header.removeFromRight(168).withSizeKeepingCentre(168, 30));
    header.removeFromRight(6);
    presetLabel.setBounds(header.removeFromRight(56).withSizeKeepingCentre(56, 24));
    header.removeFromRight(10);
    licenseBadge.setBounds(header.removeFromRight(84).withSizeKeepingCentre(84, 24));

    auto titleCol = header;
    titleLabel.setBounds(titleCol.removeFromTop(22));
    subtitleLabel.setBounds(titleCol);

    auto footer = area.removeFromBottom(250);
    footer.reduce(16, 8);
    statusLabel.setBounds(footer.removeFromBottom(18));
    footer.removeFromBottom(4);
    auto transport = footer.removeFromBottom(32);
    const int btnGap = 8;
    const int btnW = (transport.getWidth() - btnGap * 2) / 3;
    generateButton.setBounds(transport.removeFromLeft(btnW));
    transport.removeFromLeft(btnGap);
    previewButton.setBounds(transport.removeFromLeft(btnW));
    transport.removeFromLeft(btnGap);
    exportButton.setBounds(transport);
    footer.removeFromBottom(6);

    auto dragRow = footer.removeFromBottom(32);
    dragHintLabel.setBounds(dragRow.removeFromLeft(86));
    const int chipW = dragRow.getWidth() / juce::jmax(1, dragChips.size());
    for (auto* chip : dragChips)
        chip->setBounds(dragRow.removeFromLeft(chipW).reduced(3, 2));
    footer.removeFromBottom(6);

    auto mixRow = footer.removeFromBottom(26);
    mixLabel.setBounds(mixRow.removeFromLeft(40));
    const int mixSlot = mixRow.getWidth() / 5;
    for (int i = 0; i < muteButtons.size(); ++i)
    {
        auto slot = mixRow.removeFromLeft(mixSlot);
        mixPartLabels[i]->setBounds(slot.removeFromLeft(52));
        muteButtons[i]->setBounds(slot.removeFromLeft(26).reduced(1));
        slot.removeFromLeft(3);
        soloButtons[i]->setBounds(slot.removeFromLeft(26).reduced(1));
    }
    footer.removeFromBottom(6);
    clipView.setBounds(footer);

    tabs.setBounds(area.reduced(12, 8));
    layoutGeneratePage();
    layoutPlanPage();
    layoutImportPage();
    layoutStylePage();
}

void SynthesiaAIEditor::layoutGeneratePage()
{
    auto area = generatePage.getLocalBounds().reduced(12, 10);
    auto layoutCell = [](juce::Rectangle<int> cell, juce::Label& label, juce::Component& control)
    {
        label.setBounds(cell.removeFromLeft(labelWidth));
        control.setBounds(cell);
    };
    const int columnGap = 20;
    auto twoColumnRow = [&](juce::Label& leftLabel, juce::Component& left,
                            juce::Label& rightLabel, juce::Component& right)
    {
        auto row = area.removeFromTop(28);
        auto leftCell = row.removeFromLeft((row.getWidth() - columnGap) / 2);
        row.removeFromLeft(columnGap);
        layoutCell(leftCell, leftLabel, left);
        layoutCell(row, rightLabel, right);
        area.removeFromTop(8);
    };

    twoColumnRow(keyLabel, keyComboBox, scaleLabel, scaleComboBox);
    twoColumnRow(genreLabel, genreComboBox, partLabel, partComboBox);
    twoColumnRow(formLabel, formComboBox, lengthLabel, lengthSlider);
    twoColumnRow(tempoLabel, tempoSlider, tempoSyncLabel, tempoSyncComboBox);

    {
        auto row = area.removeFromTop(28);
        layoutCell(row, generatorLabel, generatorComboBox);
        area.removeFromTop(8);
    }

    {
        auto row = area.removeFromTop(28);
        layoutCell(row.removeFromLeft((row.getWidth() - columnGap) / 2), chordsLabel, chordsEditor);
        row.removeFromLeft(columnGap);
        lockProgressionButton.setBounds(row);
        area.removeFromTop(8);
    }

    {
        auto row = area.removeFromTop(24);
        locksLabel.setBounds(row.removeFromLeft(labelWidth));
        const int lockW = row.getWidth() / juce::jmax(1, partLockButtons.size());
        for (auto* button : partLockButtons)
            button->setBounds(row.removeFromLeft(lockW));
        area.removeFromTop(8);
    }

    styleLabel.setBounds(area.removeFromTop(20));
    styleTextEditor.setBounds(area);
}

void SynthesiaAIEditor::layoutPlanPage()
{
    auto area = planPage.getLocalBounds().reduced(12, 10);
    planLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);
    planButton.setBounds(area.removeFromBottom(36));
    area.removeFromBottom(10);
    planSummaryEditor.setBounds(area);
}

void SynthesiaAIEditor::layoutImportPage()
{
    auto area = importPage.getLocalBounds().reduced(12, 10);
    auto row = area.removeFromTop(28);
    importModeLabel.setBounds(row.removeFromLeft(labelWidth));
    importModeComboBox.setBounds(row);
    area.removeFromTop(12);
    row = area.removeFromTop(36);
    importButton.setBounds(row.removeFromLeft((row.getWidth() - 10) / 2));
    row.removeFromLeft(10);
    captureButton.setBounds(row);
    area.removeFromTop(16);
    importHintLabel.setBounds(area.removeFromTop(80));
}

void SynthesiaAIEditor::layoutStylePage()
{
    auto area = stylePage.getLocalBounds().reduced(12, 10);
    auto row = area.removeFromTop(28);
    styleProfileLabel.setBounds(row.removeFromLeft(labelWidth));
    styleProfileComboBox.setBounds(row);
    area.removeFromTop(10);
    row = area.removeFromTop(28);
    blendLabel.setBounds(row.removeFromLeft(labelWidth));
    blendSlider.setBounds(row);
    area.removeFromTop(12);
    learnStyleButton.setBounds(area.removeFromTop(36));
    area.removeFromTop(16);
    styleHintLabel.setBounds(area.removeFromTop(72));
}

void SynthesiaAIEditor::timerCallback()
{
    updateUIState();
}

MidiGenerationParams SynthesiaAIEditor::collectParams() const
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
    params.form = formComboBox.getSelectedId() == 2 ? ArrangementForm::Song : ArrangementForm::Loop;
    params.chordText = chordsEditor.getText().toStdString();
    params.lockProgression = lockProgressionButton.getToggleState() || !params.chordText.empty();
    params.lockedParts = 0;
    for (int i = 0; i < partLockButtons.size(); ++i)
        if (partLockButtons[i]->getToggleState())
            params.lockedParts |= partLockBit(allParts[static_cast<size_t>(i)]);
    switch (importModeComboBox.getSelectedId())
    {
        case 2: params.importMode = ImportMode::Accompany; break;
        case 3: params.importMode = ImportMode::Extend; break;
        case 4: params.importMode = ImportMode::Rewrite; break;
        case 5: params.importMode = ImportMode::Restyle; break;
        default: params.importMode = ImportMode::None; break;
    }
    params.styleBlend = blendSlider.getValue() / 100.0;
    if (styleProfileComboBox.getSelectedId() > 1)
    {
        params.styleProfileName = styleProfileComboBox.getText().toStdString();
        params.styleProfileJson = audioProcessor.getCurrentStyleJson().toStdString();
    }
    return params;
}

void SynthesiaAIEditor::snapshotParams()
{
    audioProcessor.setLastUiParams(collectParams());
}

void SynthesiaAIEditor::applyParamsToUI(const MidiGenerationParams& params)
{
    applyingPreset = true;
    keyComboBox.setSelectedId(params.rootNote + 1, juce::dontSendNotification);
    scaleComboBox.setSelectedId(static_cast<int>(params.scale) + 1, juce::dontSendNotification);
    genreComboBox.setSelectedId(static_cast<int>(params.genre) + 1, juce::dontSendNotification);
    partComboBox.setSelectedId(static_cast<int>(params.part) + 1, juce::dontSendNotification);
    formComboBox.setSelectedId(params.form == ArrangementForm::Song ? 2 : 1, juce::dontSendNotification);
    lengthSlider.setValue(params.lengthInBars, juce::dontSendNotification);
    tempoSlider.setValue(params.tempo, juce::dontSendNotification);
    tempoSyncComboBox.setSelectedId(static_cast<int>(params.quantization) + 1, juce::dontSendNotification);
    styleTextEditor.setText(params.stylePrompt);
    chordsEditor.setText(params.chordText);
    lockProgressionButton.setToggleState(params.lockProgression, juce::dontSendNotification);
    for (int i = 0; i < partLockButtons.size(); ++i)
        partLockButtons[i]->setToggleState(isPartLocked(params.lockedParts, allParts[static_cast<size_t>(i)]),
                                           juce::dontSendNotification);
    importModeComboBox.setSelectedId(static_cast<int>(params.importMode) + 1, juce::dontSendNotification);
    blendSlider.setValue(params.styleBlend * 100.0, juce::dontSendNotification);
    generatorComboBox.setSelectedId(generatorComboId(audioProcessor.getCurrentGeneratorType()),
                                    juce::dontSendNotification);
    applyingPreset = false;
}

void SynthesiaAIEditor::refreshPresetCombo()
{
    const auto current = audioProcessor.getCurrentPresetName();
    presetComboBox.clear(juce::dontSendNotification);
    int id = 1;
    int selected = 0;
    presetComboBox.addSectionHeading("Factory");
    for (const auto& preset : PresetStore::factoryPresets())
    {
        presetComboBox.addItem(preset.name, id);
        if (preset.name == current || preset.id == PresetStore::sanitiseId(current))
            selected = id;
        ++id;
    }
    const auto users = PresetStore::listUserIds();
    if (!users.isEmpty())
        presetComboBox.addSectionHeading("User");
    for (const auto& userId : users)
    {
        auto loaded = PresetStore::loadUser(userId);
        const auto label = loaded ? loaded->name : userId;
        presetComboBox.addItem(label, id);
        if (label == current || userId == PresetStore::sanitiseId(current))
            selected = id;
        ++id;
    }
    if (selected > 0)
        presetComboBox.setSelectedId(selected, juce::dontSendNotification);
}

void SynthesiaAIEditor::applyPresetFromCombo()
{
    if (applyingPreset)
        return;
    const auto name = presetComboBox.getText();
    if (name.isEmpty())
        return;
    if (!audioProcessor.applyPreset(name))
        return;
    shownPresetVersion = audioProcessor.getPresetVersion();
    shownUiParamsVersion = audioProcessor.getUiParamsVersion();
    applyParamsToUI(audioProcessor.getLastUiParams());
    statusLabel.setText("Loaded preset " + name, juce::dontSendNotification);
}

void SynthesiaAIEditor::onSavePresetClicked()
{
    snapshotParams();
    auto* window = new juce::AlertWindow(
        "Save preset",
        "Stores the current generator settings (key, genre, form, chords, prompt, style blend).",
        juce::AlertWindow::NoIcon);
    window->addTextEditor("name", audioProcessor.getCurrentPresetName(), "Preset name");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, window](int result)
        {
            if (result != 1)
                return;
            const auto name = window->getTextEditorContents("name");
            if (!audioProcessor.saveUserPreset(name))
            {
                statusLabel.setText("Could not save preset", juce::dontSendNotification);
                return;
            }
            shownPresetVersion = audioProcessor.getPresetVersion();
            refreshPresetCombo();
            statusLabel.setText("Saved preset " + name, juce::dontSendNotification);
        }), true);
}

void SynthesiaAIEditor::startExternalMidiDrag(Part part)
{
    if (audioProcessor.getGeneratedMidi().isEmpty())
        return;

    const auto file = audioProcessor.writeTempMidi(part);
    if (!file.existsAsFile())
    {
        statusLabel.setText("Could not write a temp MIDI file", juce::dontSendNotification);
        return;
    }

    juce::StringArray files;
    files.add(file.getFullPathName());
    performExternalDragDropOfFiles(files, false, this);
}

void SynthesiaAIEditor::updateLicenseBadge()
{
    const bool licensed = audioProcessor.isLicensed();
    licenseBadge.setText(licensed ? "LICENSED" : "TRIAL", juce::dontSendNotification);
    licenseBadge.setColour(juce::Label::backgroundColourId,
                           licensed ? Theme::licensed : Theme::trial);
    licenseBadge.setColour(juce::Label::textColourId, juce::Colours::black);
}

void SynthesiaAIEditor::applyPlanToUI(const ArrangementPlan& plan)
{
    genreComboBox.setSelectedId(static_cast<int>(plan.genre) + 1, juce::dontSendNotification);
    keyComboBox.setSelectedId(plan.rootNote + 1, juce::dontSendNotification);
    scaleComboBox.setSelectedId(static_cast<int>(plan.scale) + 1, juce::dontSendNotification);
    tempoSlider.setValue(plan.tempo, juce::dontSendNotification);
    lengthSlider.setValue(plan.lengthInBars, juce::dontSendNotification);
    formComboBox.setSelectedId(plan.sections.size() > 1 ? 2 : 1, juce::dontSendNotification);
    chordsEditor.setText(plan.chordChart(), juce::dontSendNotification);
    chordsEditor.setCaretPosition(0);
    lockProgressionButton.setToggleState(true, juce::dontSendNotification);
    planSummaryEditor.setText(plan.summary());
}

void SynthesiaAIEditor::applyImportToUI(const MidiAnalysis& analysis)
{
    if (!analysis.ok)
    {
        statusLabel.setText(analysis.error.isEmpty() ? "Import failed" : analysis.error,
                            juce::dontSendNotification);
        return;
    }

    keyComboBox.setSelectedId(analysis.rootNote + 1, juce::dontSendNotification);
    scaleComboBox.setSelectedId(static_cast<int>(analysis.scale) + 1, juce::dontSendNotification);
    tempoSlider.setValue(analysis.tempo, juce::dontSendNotification);
    if (importModeComboBox.getSelectedId() != 3)
        lengthSlider.setValue(analysis.lengthInBars, juce::dontSendNotification);
    chordsEditor.setText(analysis.chordChart, juce::dontSendNotification);
    chordsEditor.setCaretPosition(0);
    lockProgressionButton.setToggleState(true, juce::dontSendNotification);

    for (int i = 0; i < partLockButtons.size(); ++i)
        partLockButtons[i]->setToggleState(allParts[static_cast<size_t>(i)] == analysis.primaryPart,
                                           juce::dontSendNotification);

    planSummaryEditor.setText(analysis.summary);
    statusLabel.setText("Imported " + analysis.sourceName + " · "
                            + juce::String(partName(analysis.primaryPart)) + " · "
                            + juce::String(analysis.lengthInBars) + " bars",
                        juce::dontSendNotification);
}

void SynthesiaAIEditor::onPlanClicked()
{
    audioProcessor.planArrangement(collectParams());
    statusLabel.setText("Planning...", juce::dontSendNotification);
}

void SynthesiaAIEditor::onGenerateClicked()
{
    audioProcessor.generateMidi(collectParams());
    statusLabel.setText("Generating...", juce::dontSendNotification);
}

bool SynthesiaAIEditor::importPath(const juce::File& file)
{
    if (!audioProcessor.importMidiFile(file))
    {
        statusLabel.setText(audioProcessor.getLastImport().error, juce::dontSendNotification);
        return false;
    }
    applyImportToUI(audioProcessor.getLastImport());
    shownImportVersion = audioProcessor.getImportVersion();
    shownClipVersion = audioProcessor.getClipVersion();
    clipView.setClip(audioProcessor.getGeneratedMidi(),
                     audioProcessor.getClipLengthInBeats(),
                     audioProcessor.getProgression(),
                     audioProcessor.getSections());
    return true;
}

void SynthesiaAIEditor::onImportClicked()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import MIDI file...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.mid;*.midi");
    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file != juce::File{})
            importPath(file);
    });
}

void SynthesiaAIEditor::onCaptureClicked()
{
    if (audioProcessor.isCapturingMidi())
    {
        const bool ok = audioProcessor.finishMidiCapture();
        captureButton.setButtonText("Capture MIDI");
        if (ok)
        {
            applyImportToUI(audioProcessor.getLastImport());
            shownImportVersion = audioProcessor.getImportVersion();
            shownClipVersion = audioProcessor.getClipVersion();
            clipView.setClip(audioProcessor.getGeneratedMidi(),
                             audioProcessor.getClipLengthInBeats(),
                             audioProcessor.getProgression(),
                             audioProcessor.getSections());
        }
        else
        {
            statusLabel.setText("Capture was empty - play MIDI into the plugin first",
                                juce::dontSendNotification);
        }
        return;
    }

    audioProcessor.setMidiCapture(true);
    captureButton.setButtonText("Stop capture");
    statusLabel.setText("Capturing host MIDI...", juce::dontSendNotification);
}

bool SynthesiaAIEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File file(path);
        if (file.hasFileExtension("mid") || file.hasFileExtension("midi") || file.isDirectory())
            return true;
    }
    return false;
}

void SynthesiaAIEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    draggingFiles = true;
    repaint();
}

void SynthesiaAIEditor::fileDragExit(const juce::StringArray&)
{
    draggingFiles = false;
    repaint();
}

void SynthesiaAIEditor::filesDropped(const juce::StringArray& files, int, int)
{
    draggingFiles = false;
    repaint();

    juce::Array<juce::File> mids;
    juce::File folder;
    for (const auto& path : files)
    {
        const juce::File file(path);
        if (file.isDirectory())
            folder = file;
        else if (file.hasFileExtension("mid") || file.hasFileExtension("midi"))
            mids.add(file);
    }

    if (folder != juce::File{})
    {
        learnFromFiles({ folder }, folder.getFileName());
        return;
    }
    if (mids.size() > 1)
    {
        learnFromFiles(mids, mids.getFirst().getParentDirectory().getFileName());
        return;
    }
    if (mids.size() == 1)
        importPath(mids.getFirst());
}

void SynthesiaAIEditor::refreshStyleCombo()
{
    const auto current = audioProcessor.getCurrentStyleName();
    styleProfileComboBox.clear(juce::dontSendNotification);
    styleProfileComboBox.addItem("None", 1);
    const auto names = audioProcessor.listStyleProfiles();
    int selected = 1;
    for (int i = 0; i < names.size(); ++i)
    {
        styleProfileComboBox.addItem(names[i], i + 2);
        if (names[i] == current)
            selected = i + 2;
    }
    styleProfileComboBox.setSelectedId(selected, juce::dontSendNotification);
}

void SynthesiaAIEditor::learnFromFiles(const juce::Array<juce::File>& files, const juce::String& suggestedName)
{
    auto* window = new juce::AlertWindow(
        "Learn style profile",
        "Extract chord vocabulary, voicings, rhythm, bass approaches and swing from the selected MIDI.",
        juce::AlertWindow::NoIcon);
    window->addTextEditor("name", StyleProfile::sanitiseName(suggestedName), "Profile name");
    window->addButton("Learn", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, files, window](int result)
        {
            if (result != 1)
                return;
            const auto name = window->getTextEditorContents("name");
            if (!audioProcessor.learnStyle(files, name))
            {
                statusLabel.setText("Could not learn a style from those files",
                                    juce::dontSendNotification);
                return;
            }
            shownStyleVersion = audioProcessor.getStyleVersion();
            refreshStyleCombo();
            blendSlider.setValue(audioProcessor.getStyleBlend() * 100.0, juce::dontSendNotification);
            planSummaryEditor.setText(audioProcessor.getCurrentStyle().summary);
            statusLabel.setText("Learned style " + audioProcessor.getCurrentStyleName(),
                                juce::dontSendNotification);
        }), true);
}

void SynthesiaAIEditor::onLearnStyleClicked()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Learn style from MIDI files or a folder...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.mid;*.midi");
    const auto chooserFlags = juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectDirectories
                            | juce::FileBrowserComponent::canSelectMultipleItems;
    chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc)
    {
        auto results = fc.getResults();
        if (results.isEmpty())
        {
            const auto single = fc.getResult();
            if (single != juce::File{})
                results.add(single);
        }
        if (results.isEmpty())
            return;
        juce::String suggested = results.getFirst().isDirectory()
            ? results.getFirst().getFileName()
            : results.getFirst().getParentDirectory().getFileName();
        learnFromFiles(results, suggested);
    });
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
    planButton.setEnabled(!isGenerating);

    bool hasMidi = !audioProcessor.getGeneratedMidi().isEmpty();
    exportButton.setEnabled(hasMidi && !isGenerating);
    previewButton.setEnabled(hasMidi && !isGenerating);
    previewButton.setButtonText(audioProcessor.isPreviewPlaying() ? "Stop Preview" : "Play Preview");
    for (auto* chip : dragChips)
        chip->setEnabledLook(hasMidi && !isGenerating);

    captureButton.setButtonText(audioProcessor.isCapturingMidi() ? "Stop capture" : "Capture MIDI");
    updateLicenseBadge();

    clipView.setMuteSolo(audioProcessor.getMuteMask(), audioProcessor.getSoloMask());
    for (int i = 0; i < muteButtons.size(); ++i)
    {
        muteButtons[i]->setToggleState(audioProcessor.isPartMuted(allParts[static_cast<size_t>(i)]),
                                       juce::dontSendNotification);
        soloButtons[i]->setToggleState(audioProcessor.isPartSoloed(allParts[static_cast<size_t>(i)]),
                                       juce::dontSendNotification);
    }

    if (shownPresetVersion != audioProcessor.getPresetVersion())
    {
        shownPresetVersion = audioProcessor.getPresetVersion();
        refreshPresetCombo();
        applyParamsToUI(audioProcessor.getLastUiParams());
        shownUiParamsVersion = audioProcessor.getUiParamsVersion();
    }

    if (shownUiParamsVersion != audioProcessor.getUiParamsVersion() && !isGenerating)
    {
        shownUiParamsVersion = audioProcessor.getUiParamsVersion();
    }

    if (shownStyleVersion != audioProcessor.getStyleVersion())
    {
        shownStyleVersion = audioProcessor.getStyleVersion();
        refreshStyleCombo();
        blendSlider.setValue(audioProcessor.getStyleBlend() * 100.0, juce::dontSendNotification);
        if (!audioProcessor.getCurrentStyle().empty())
            planSummaryEditor.setText(audioProcessor.getCurrentStyle().summary);
    }

    if (shownImportVersion != audioProcessor.getImportVersion())
    {
        shownImportVersion = audioProcessor.getImportVersion();
        applyImportToUI(audioProcessor.getLastImport());
    }

    if (shownPlanVersion != audioProcessor.getPlanVersion())
    {
        shownPlanVersion = audioProcessor.getPlanVersion();
        const auto& plan = audioProcessor.getLastPlan();
        if (!plan.sections.empty())
            applyPlanToUI(plan);
    }

    if (shownClipVersion != audioProcessor.getClipVersion())
    {
        shownClipVersion = audioProcessor.getClipVersion();
        clipView.setClip(audioProcessor.getGeneratedMidi(),
                         audioProcessor.getClipLengthInBeats(),
                         audioProcessor.getProgression(),
                         audioProcessor.getSections());
        clipView.setMuteSolo(audioProcessor.getMuteMask(), audioProcessor.getSoloMask());
    }
    clipView.setPlaybackBeat(audioProcessor.getPlaybackBeat());

    if (!isGenerating && statusLabel.getText() == "Generating...")
        statusLabel.setText(hasMidi ? "Generation complete - drag a part into your DAW"
                                    : "Generation failed (check generator settings)",
                            juce::dontSendNotification);
    if (!isGenerating && statusLabel.getText() == "Planning...")
    {
        const auto& plan = audioProcessor.getLastPlan();
        juce::String text = "Planned (" + juce::String(plan.source) + ")";
        if (plan.usage.totalTokens > 0)
            text << " · " << plan.usage.promptTokens << " prompt / "
                 << plan.usage.completionTokens << " completion tokens";
        else
            text << " · offline heuristic (no API key)";
        statusLabel.setText(text, juce::dontSendNotification);
        tabs.setCurrentTabIndex(1);
    }
}

SettingsDialog::SettingsDialog(SynthesiaAIProcessor& p)
    : processor(p)
{
    auto styleEditor = [](juce::TextEditor& editor)
    {
        editor.setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground);
        editor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
        editor.setColour(juce::TextEditor::textColourId, Theme::label);
        editor.setFont(juce::Font(15.0f));
    };

    setUpLabel(providerLabel, "Provider");
    addAndMakeVisible(providerLabel);
    providerComboBox.addItem("OpenAI", 1);
    providerComboBox.addItem("Groq", 2);
    providerComboBox.addItem("Custom", 3);
    const auto provider = processor.getApiProvider();
    if (provider.equalsIgnoreCase("Groq"))
        providerComboBox.setSelectedId(2, juce::dontSendNotification);
    else if (provider.equalsIgnoreCase("Custom"))
        providerComboBox.setSelectedId(3, juce::dontSendNotification);
    else
        providerComboBox.setSelectedId(1, juce::dontSendNotification);
    providerComboBox.onChange = [this]() { applyProviderDefaults(); };
    addAndMakeVisible(providerComboBox);

    setUpLabel(apiKeyLabel, "API key");
    addAndMakeVisible(apiKeyLabel);

    apiKeyEditor.setText(processor.getApiKey());
    apiKeyEditor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    apiKeyEditor.setTextToShowWhenEmpty("sk-... or gsk-... (leave empty for offline heuristic)", Theme::dimText);
    styleEditor(apiKeyEditor);
    addAndMakeVisible(apiKeyEditor);

    setUpLabel(endpointLabel, "API base URL");
    addAndMakeVisible(endpointLabel);
    endpointEditor.setText(processor.getApiEndpoint());
    endpointEditor.setTextToShowWhenEmpty("https://api.openai.com/v1/chat/completions", Theme::dimText);
    styleEditor(endpointEditor);
    addAndMakeVisible(endpointEditor);

    setUpLabel(modelLabel, "Model");
    addAndMakeVisible(modelLabel);
    modelEditor.setText(processor.getApiModel());
    modelEditor.setTextToShowWhenEmpty("gpt-4o-mini", Theme::dimText);
    styleEditor(modelEditor);
    addAndMakeVisible(modelEditor);

    setUpLabel(licenseLabel, "License key");
    addAndMakeVisible(licenseLabel);
    licenseEditor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    licenseEditor.setText(processor.getLicenseKey());
    licenseEditor.setTextToShowWhenEmpty("SYN-... (optional, offline stub)", Theme::dimText);
    styleEditor(licenseEditor);
    addAndMakeVisible(licenseEditor);

    const auto last = processor.getLastTokenUsage();
    const auto session = processor.getSessionTokenUsage();
    juce::String usage;
    usage << "Last request: " << last.promptTokens << " prompt / "
          << last.completionTokens << " completion (" << last.totalTokens << " total)\n"
          << "Session: " << session.promptTokens << " prompt / "
          << session.completionTokens << " completion · " << session.requests << " calls";
    usageLabel.setText(usage, juce::dontSendNotification);
    usageLabel.setFont(juce::Font(13.0f));
    usageLabel.setColour(juce::Label::textColourId, Theme::label);
    usageLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(usageLabel);

    hintLabel.setText("Keys stay on this machine (user app data) and are never logged. "
                      "Without a key, Plan maps prompt keywords offline. Groq uses "
                      "https://api.groq.com/openai/v1 and model openai/gpt-oss-120b. "
                      "A license key starting with SYN- (12+ chars) swaps the Trial badge.",
                      juce::dontSendNotification);
    hintLabel.setFont(juce::Font(13.0f));
    hintLabel.setColour(juce::Label::textColourId, Theme::dimText);
    hintLabel.setJustificationType(juce::Justification::topLeft);
    hintLabel.setMinimumHorizontalScale(1.0f);
    addAndMakeVisible(hintLabel);

    saveButton.setButtonText("Save");
    saveButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    saveButton.onClick = [this]()
    {
        const auto providerId = providerComboBox.getSelectedId();
        processor.setApiProvider(providerId == 2 ? "Groq" : (providerId == 3 ? "Custom" : "OpenAI"));
        processor.setApiKey(apiKeyEditor.getText());
        processor.setApiEndpoint(endpointEditor.getText());
        processor.setApiModel(modelEditor.getText());
        processor.setLicenseKey(licenseEditor.getText());
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

void SettingsDialog::applyProviderDefaults()
{
    const int id = providerComboBox.getSelectedId();
    if (id == 2)
    {
        endpointEditor.setText(SynthesiaAIProcessor::groqChatUrl(), juce::dontSendNotification);
        modelEditor.setText(SynthesiaAIProcessor::groqDefaultModel(), juce::dontSendNotification);
    }
    else if (id == 1)
    {
        endpointEditor.setText(SynthesiaAIProcessor::openaiChatUrl(), juce::dontSendNotification);
        modelEditor.setText("gpt-4o-mini", juce::dontSendNotification);
    }
}

void SettingsDialog::paint(juce::Graphics& g)
{
    g.fillAll(Theme::background);
}

void SettingsDialog::resized()
{
    auto area = getLocalBounds().reduced(20);

    providerLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(2);
    providerComboBox.setBounds(area.removeFromTop(28));
    area.removeFromTop(8);

    auto field = [&area](juce::Label& label, juce::TextEditor& editor)
    {
        label.setBounds(area.removeFromTop(20));
        area.removeFromTop(2);
        editor.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
    };

    field(apiKeyLabel, apiKeyEditor);
    field(endpointLabel, endpointEditor);
    field(modelLabel, modelEditor);
    field(licenseLabel, licenseEditor);

    usageLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(6);
    hintLabel.setBounds(area.removeFromTop(72));

    auto buttonRow = area.removeFromBottom(32);
    saveButton.setBounds(buttonRow.removeFromRight(96));
    buttonRow.removeFromRight(10);
    cancelButton.setBounds(buttonRow.removeFromRight(96));
}
