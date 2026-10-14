#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MidiExporter.h"
#include "Generators/GenreProfiles.h"
#include "PresetStore.h"
#include "LicenseStub.h"
#include "Inspire.h"
#include "FontBinaryData.h"
#include <cmath>
#include <map>

namespace
{
    const char* keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const char* scaleNames[] = { "Major", "Minor", "Dorian", "Mixolydian", "Pentatonic" };
    const char* shortGenre[] = { "Pop", "Lo-fi", "House", "Trap", "R&B", "Film" };

    juce::Colour colourForChannel(int channel)
    {
        switch (channel)
        {
            case 1:  return juce::Colour(0xfff0c27a);
            case 2:  return juce::Colour(0xffff6b7a);
            case 3:  return juce::Colour(0xff9a7cff);
            case 4:  return juce::Colour(0xff7dcea0);
            case 10: return juce::Colour(0xff5ee0c4);
            default: return Theme::accent;
        }
    }

    juce::Colour colourForGenre(Genre genre)
    {
        switch (genre)
        {
            case Genre::Pop:       return juce::Colour(0xff9a7cff);
            case Genre::LoFiJazz:  return juce::Colour(0xfff0c27a);
            case Genre::House:     return juce::Colour(0xff5ee0c4);
            case Genre::Trap:      return juce::Colour(0xffff6b7a);
            case Genre::RnB:       return juce::Colour(0xfff0a0c8);
            case Genre::Cinematic: return juce::Colour(0xffa8c4ff);
        }
        return Theme::accent;
    }

    juce::Typeface::Ptr regularFace()
    {
        static auto face = juce::Typeface::createSystemTypefaceFor(
            FontBinaryData::SpaceGroteskRegular_ttf, FontBinaryData::SpaceGroteskRegular_ttfSize);
        return face;
    }

    juce::Typeface::Ptr mediumFace()
    {
        static auto face = juce::Typeface::createSystemTypefaceFor(
            FontBinaryData::SpaceGroteskMedium_ttf, FontBinaryData::SpaceGroteskMedium_ttfSize);
        return face;
    }

    void drawGenreMark(juce::Graphics& g, Genre genre, juce::Rectangle<float> b, juce::Colour colour)
    {
        auto r = b.reduced(b.getWidth() * 0.18f, b.getHeight() * 0.18f);
        juce::Path p;
        const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
        switch (genre)
        {
            case Genre::Pop:
                p.addTriangle(x + w * 0.5f, y, x + w * 0.62f, y + h * 0.38f, x + w * 0.38f, y + h * 0.38f);
                p.addTriangle(x + w * 0.5f, y + h, x + w * 0.62f, y + h * 0.62f, x + w * 0.38f, y + h * 0.62f);
                p.addTriangle(x, y + h * 0.5f, x + w * 0.38f, y + h * 0.38f, x + w * 0.38f, y + h * 0.62f);
                p.addTriangle(x + w, y + h * 0.5f, x + w * 0.62f, y + h * 0.38f, x + w * 0.62f, y + h * 0.62f);
                break;
            case Genre::LoFiJazz:
                p.addCentredArc(x + w * 0.55f, y + h * 0.55f, w * 0.42f, h * 0.42f, 0.0f, 0.6f, 5.4f, true);
                p.addEllipse(x + w * 0.58f, y + h * 0.12f, w * 0.28f, h * 0.28f);
                break;
            case Genre::House:
                p.addRoundedRectangle(x + w * 0.18f, y + h * 0.42f, w * 0.64f, h * 0.46f, 2.0f);
                p.addTriangle(x + w * 0.08f, y + h * 0.46f, x + w * 0.5f, y, x + w * 0.92f, y + h * 0.46f);
                break;
            case Genre::Trap:
                p.addTriangle(x + w * 0.5f, y, x + w, y + h * 0.5f, x + w * 0.5f, y + h);
                p.addTriangle(x + w * 0.5f, y, x, y + h * 0.5f, x + w * 0.5f, y + h);
                break;
            case Genre::RnB:
                p.addCentredArc(x + w * 0.32f, y + h * 0.38f, w * 0.22f, h * 0.22f, 0.0f, 3.3f, 6.2f, true);
                p.addCentredArc(x + w * 0.68f, y + h * 0.38f, w * 0.22f, h * 0.22f, 0.0f, 3.3f, 6.2f, true);
                p.startNewSubPath(x + w * 0.08f, y + h * 0.42f);
                p.quadraticTo(x + w * 0.5f, y + h * 1.08f, x + w * 0.92f, y + h * 0.42f);
                break;
            case Genre::Cinematic:
                p.addRoundedRectangle(x, y + h * 0.18f, w, h * 0.64f, 2.0f);
                p.addRectangle(x + w * 0.12f, y + h * 0.04f, w * 0.16f, h * 0.14f);
                p.addRectangle(x + w * 0.72f, y + h * 0.04f, w * 0.16f, h * 0.14f);
                break;
        }
        g.setColour(colour);
        g.fillPath(p);
    }

    void drawGlowPill(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour, float glow)
    {
        if (glow > 0.02f)
        {
            g.setColour(colour.withAlpha(0.10f * glow));
            g.fillRoundedRectangle(r.expanded(3.4f, 2.6f), 5.0f);
            g.setColour(colour.withAlpha(0.20f * glow));
            g.fillRoundedRectangle(r.expanded(1.6f, 1.2f), 4.0f);
        }
        g.setColour(colour);
        g.fillRoundedRectangle(r, 3.2f);
        auto sheen = r.removeFromTop(juce::jmax(1.2f, r.getHeight() * 0.38f)).reduced(0.7f, 0.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f + 0.10f * glow));
        g.fillRoundedRectangle(sheen, 2.0f);
    }

    void tickToward(float& value, float target, float rate = 0.32f)
    {
        value += (target - value) * rate;
    }

    juce::Colour colourForPart(Part part) { return colourForChannel(partChannel(part)); }

    const char* laneName(Part part)
    {
        switch (part)
        {
            case Part::Chords: return "Chords";
            case Part::Bass:   return "Bass";
            case Part::Synth:  return "Melody";
            case Part::Piano:  return "Piano";
            case Part::Drums:  return "Drums";
            default:           return "All";
        }
    }

    void setupLabel(juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, Theme::dimText);
        label.setFont(Theme::regular(13.0f));
    }

}

juce::Font Theme::regular(float height)
{
    return juce::Font(regularFace()).withHeight(height);
}

juce::Font Theme::medium(float height)
{
    return juce::Font(mediumFace()).withHeight(height);
}

void Theme::fillGlass(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    g.setColour(Theme::surface.withAlpha(0.86f));
    g.fillRoundedRectangle(bounds, radius);
    juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.08f), bounds.getX(), bounds.getY(),
                               juce::Colours::transparentWhite, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(sheen);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

void Theme::fillAccentGradient(juce::Graphics& g, juce::Rectangle<float> bounds, float alpha)
{
    juce::ColourGradient grad(Theme::accentViolet.withAlpha(alpha), bounds.getX(), bounds.getY(),
                              Theme::accentCoral.withAlpha(alpha), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 12.0f);
}

void drawIcon(juce::Graphics& g, IconKind kind, juce::Rectangle<float> b, juce::Colour colour)
{
    auto r = b.reduced(b.getWidth() * 0.22f, b.getHeight() * 0.22f);
    juce::Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

    switch (kind)
    {
        case IconKind::Unmute:
            p.addTriangle(x, y + h * 0.3f, x + w * 0.38f, y + h * 0.3f, x + w * 0.38f, y + h * 0.7f);
            p.addRectangle(x, y + h * 0.3f, w * 0.22f, h * 0.4f);
            p.startNewSubPath(x + w * 0.55f, y + h * 0.32f);
            p.quadraticTo(x + w, y + h * 0.5f, x + w * 0.55f, y + h * 0.68f);
            break;
        case IconKind::Mute:
            p.addTriangle(x, y + h * 0.3f, x + w * 0.38f, y + h * 0.3f, x + w * 0.38f, y + h * 0.7f);
            p.addRectangle(x, y + h * 0.3f, w * 0.22f, h * 0.4f);
            p.startNewSubPath(x + w * 0.55f, y + h * 0.28f);
            p.lineTo(x + w * 0.92f, y + h * 0.72f);
            p.startNewSubPath(x + w * 0.92f, y + h * 0.28f);
            p.lineTo(x + w * 0.55f, y + h * 0.72f);
            break;
        case IconKind::Regen:
            p.addCentredArc(x + w * 0.5f, y + h * 0.5f, w * 0.38f, h * 0.38f, 0.0f, 0.4f, 3.6f, true);
            p.startNewSubPath(x + w * 0.78f, y + h * 0.18f);
            p.lineTo(x + w * 0.92f, y + h * 0.38f);
            p.lineTo(x + w * 0.62f, y + h * 0.36f);
            break;
        case IconKind::Drag:
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 3; ++col)
                    p.addEllipse(x + col * w * 0.32f, y + h * 0.22f + row * h * 0.38f, w * 0.18f, h * 0.18f);
            break;
        case IconKind::Play:
            p.addTriangle(x + w * 0.18f, y, x + w * 0.18f, y + h, x + w * 0.92f, y + h * 0.5f);
            break;
        case IconKind::Stop:
            p.addRoundedRectangle(x + w * 0.12f, y + h * 0.12f, w * 0.76f, h * 0.76f, 2.0f);
            break;
        case IconKind::Settings:
        {
            const auto c = r.getCentre();
            p.addEllipse(c.x - w * 0.16f, c.y - h * 0.16f, w * 0.32f, h * 0.32f);
            for (int i = 0; i < 6; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::pi / 3.0f;
                juce::Path tooth;
                tooth.addRoundedRectangle(-w * 0.08f, -h * 0.48f, w * 0.16f, h * 0.22f, 1.2f);
                tooth.applyTransform(juce::AffineTransform::rotation(a).translated(c.x, c.y));
                p.addPath(tooth);
            }
            break;
        }
        case IconKind::Chevron:
            p.startNewSubPath(x + w * 0.15f, y + h * 0.32f);
            p.lineTo(x + w * 0.5f, y + h * 0.68f);
            p.lineTo(x + w * 0.85f, y + h * 0.32f);
            break;
        case IconKind::Import:
            p.startNewSubPath(x + w * 0.5f, y);
            p.lineTo(x + w * 0.5f, y + h * 0.58f);
            p.startNewSubPath(x + w * 0.28f, y + h * 0.36f);
            p.lineTo(x + w * 0.5f, y + h * 0.62f);
            p.lineTo(x + w * 0.72f, y + h * 0.36f);
            p.startNewSubPath(x, y + h * 0.78f);
            p.lineTo(x, y + h);
            p.lineTo(x + w, y + h);
            p.lineTo(x + w, y + h * 0.78f);
            break;
        case IconKind::Close:
            p.startNewSubPath(x, y);
            p.lineTo(x + w, y + h);
            p.startNewSubPath(x + w, y);
            p.lineTo(x, y + h);
            break;
        case IconKind::Dice:
            p.addRoundedRectangle(x, y, w, h, 2.4f);
            p.addEllipse(x + w * 0.18f, y + h * 0.18f, w * 0.18f, h * 0.18f);
            p.addEllipse(x + w * 0.41f, y + h * 0.41f, w * 0.18f, h * 0.18f);
            p.addEllipse(x + w * 0.64f, y + h * 0.64f, w * 0.18f, h * 0.18f);
            break;
        case IconKind::Sparkle:
            p.addTriangle(x + w * 0.5f, y, x + w * 0.62f, y + h * 0.38f, x + w * 0.38f, y + h * 0.38f);
            p.addTriangle(x + w * 0.5f, y + h, x + w * 0.62f, y + h * 0.62f, x + w * 0.38f, y + h * 0.62f);
            p.addTriangle(x, y + h * 0.5f, x + w * 0.38f, y + h * 0.38f, x + w * 0.38f, y + h * 0.62f);
            p.addTriangle(x + w, y + h * 0.5f, x + w * 0.62f, y + h * 0.38f, x + w * 0.62f, y + h * 0.62f);
            break;
        case IconKind::Back:
            p.startNewSubPath(x + w * 0.68f, y + h * 0.12f);
            p.lineTo(x + w * 0.28f, y + h * 0.5f);
            p.lineTo(x + w * 0.68f, y + h * 0.88f);
            break;
        case IconKind::Forward:
            p.startNewSubPath(x + w * 0.32f, y + h * 0.12f);
            p.lineTo(x + w * 0.72f, y + h * 0.5f);
            p.lineTo(x + w * 0.32f, y + h * 0.88f);
            break;
        case IconKind::Lock:
            p.addRoundedRectangle(x + w * 0.18f, y + h * 0.42f, w * 0.64f, h * 0.48f, 1.8f);
            p.addCentredArc(x + w * 0.5f, y + h * 0.42f, w * 0.22f, h * 0.28f, 0.0f, juce::MathConstants<float>::pi, 0.0f, true);
            break;
    }

    g.setColour(colour);
    if (kind == IconKind::Play || kind == IconKind::Stop || kind == IconKind::Drag
        || kind == IconKind::Settings || kind == IconKind::Sparkle)
        g.fillPath(p);
    else
        g.strokePath(p, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

SynthesiaLookAndFeel::SynthesiaLookAndFeel()
{
    setColourScheme({
        0xff07080d, 0xff1a1f2c, 0xff10121a, 0xff2a3144,
        0xfff3f1f7, 0xff9a7cff, 0xffffffff, 0xff9a7cff, 0xffffffff
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

void SynthesiaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = backgroundColour;
    if (down)
        colour = colour.brighter(0.10f);
    else if (highlighted)
        colour = colour.brighter(0.14f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(juce::Colours::white.withAlpha(down ? 0.04f : (highlighted ? 0.08f : 0.05f)));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);
}

void SynthesiaLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, (float) width, (float) height).reduced(0.5f);
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
    juce::Path arrow;
    const float x = (float) width - 18.0f, y = (float) height * 0.42f;
    arrow.addTriangle(x, y, x + 8.0f, y, x + 4.0f, y + 6.0f);
    g.setColour(Theme::dimText);
    g.fillPath(arrow);
}

void SynthesiaLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                                    juce::TextEditor& editor)
{
    g.setColour(editor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(0.0f, 0.0f, (float) width, (float) height, 12.0f);
}

void SynthesiaLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                                 juce::TextEditor& editor)
{
    const auto colour = editor.hasKeyboardFocus(true)
        ? editor.findColour(juce::TextEditor::focusedOutlineColourId)
        : editor.findColour(juce::TextEditor::outlineColourId);
    g.setColour(colour);
    g.drawRoundedRectangle(0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 12.0f, 1.2f);
}

juce::Font SynthesiaLookAndFeel::getTextButtonFont(juce::TextButton&, int) { return Theme::medium(14.5f); }
juce::Font SynthesiaLookAndFeel::getLabelFont(juce::Label&) { return Theme::regular(13.0f); }
juce::Font SynthesiaLookAndFeel::getComboBoxFont(juce::ComboBox&) { return Theme::regular(14.0f); }
juce::Font SynthesiaLookAndFeel::getPopupMenuFont() { return Theme::regular(14.0f); }

IconButton::IconButton(IconKind k, juce::String tip) : juce::Button({}), kind(k)
{
    setTooltip(std::move(tip));
}

void IconButton::setKind(IconKind k)
{
    kind = k;
    repaint();
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    if (highlighted || down || getToggleState())
    {
        g.setColour(Theme::accent.withAlpha(down ? 0.28f : 0.16f));
        g.fillRoundedRectangle(bounds, 8.0f);
    }
    const auto colour = getToggleState() ? Theme::accent : (highlighted ? Theme::label : Theme::dimText);
    drawIcon(g, kind, bounds, colour);
}

GenreChip::GenreChip(Genre g) : juce::Button(shortGenre[static_cast<int>(g)]), genre(g)
{
    setClickingTogglesState(false);
}

void GenreChip::tick()
{
    const float previous = hover;
    tickToward(hover, isMouseOver() || getToggleState() ? 1.0f : 0.0f);
    if (std::abs(hover - previous) > 0.01f)
        repaint();
}

void GenreChip::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool on = getToggleState();
    const auto tint = colourForGenre(genre);
    if (on)
    {
        juce::ColourGradient grad(tint, bounds.getX(), bounds.getY(),
                                  tint.interpolatedWith(Theme::accentCoral, 0.45f),
                                  bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bounds, 16.0f);
        g.setColour(juce::Colours::white.withAlpha(0.16f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 16.0f, 1.0f);
    }
    else
    {
        g.setColour(Theme::panel.brighter(0.08f * hover + (down ? 0.06f : 0.0f)));
        g.fillRoundedRectangle(bounds, 16.0f);
        g.setColour(tint.withAlpha(0.28f + 0.35f * hover));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 16.0f, 1.0f);
    }
    auto icon = bounds.removeFromLeft(22.0f).reduced(4.0f, 7.0f);
    drawGenreMark(g, genre, icon, on ? juce::Colours::white : tint.interpolatedWith(Theme::label, 0.25f));
    g.setColour(on ? juce::Colours::white : Theme::label);
    g.setFont(Theme::medium(13.0f));
    g.drawText(getButtonText(), bounds.withTrimmedRight(6.0f), juce::Justification::centred, false);
    juce::ignoreUnused(highlighted);
}

void Wordmark::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto mark = bounds.removeFromLeft(28.0f).withSizeKeepingCentre(22.0f, 22.0f);
    Theme::fillAccentGradient(g, mark, 1.0f);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    juce::Path diamond;
    diamond.addRoundedRectangle(mark.reduced(6.2f), 2.2f);
    diamond.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi * 0.25f,
                                                           mark.getCentreX(), mark.getCentreY()));
    g.fillPath(diamond);

    g.setFont(Theme::medium(20.0f));
    g.setColour(Theme::label);
    auto name = bounds.removeFromLeft(108.0f);
    g.drawText("Synthesia", name, juce::Justification::centredLeft, false);
    g.setFont(Theme::medium(20.0f));
    juce::ColourGradient grad(Theme::accentViolet, bounds.getX(), bounds.getY(),
                              Theme::accentCoral, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.drawText("AI", bounds, juce::Justification::centredLeft, false);
}

void PreviewOrb::setLevel(float newLevel)
{
    level = juce::jlimit(0.0f, 1.0f, newLevel * 2.4f);
}

void PreviewOrb::setPlaying(bool shouldPlay)
{
    playing = shouldPlay;
}

void PreviewOrb::tick()
{
    phase += playing ? 0.14f : 0.04f;
    const float previous = display;
    tickToward(display, playing ? juce::jmax(0.12f, level) : level * 0.25f, 0.28f);
    if (playing || std::abs(display - previous) > 0.008f)
        repaint();
}

void PreviewOrb::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    Theme::fillGlass(g, bounds, 16.0f);
    const auto centre = bounds.getCentre();
    const float radius = 11.0f + 7.0f * display;
    g.setColour(Theme::accentViolet.withAlpha(0.10f + 0.22f * display));
    g.fillEllipse(centre.x - radius - 6.0f, centre.y - radius - 6.0f, (radius + 6.0f) * 2.0f, (radius + 6.0f) * 2.0f);
    juce::ColourGradient orb(Theme::accentViolet, centre.x - 4.0f, centre.y - 4.0f,
                             Theme::accentCoral, centre.x + radius, centre.y + radius, true);
    g.setGradientFill(orb);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.22f + 0.25f * display));
    g.fillEllipse(centre.x - radius * 0.35f, centre.y - radius * 0.42f, radius * 0.7f, radius * 0.45f);

    auto bars = bounds.removeFromRight(22.0f).reduced(3.0f, 10.0f);
    const int count = 5;
    const float gap = 2.0f;
    const float bw = (bars.getWidth() - gap * (count - 1)) / (float) count;
    for (int i = 0; i < count; ++i)
    {
        const float wave = 0.25f + 0.75f * display * (0.45f + 0.55f * std::sin(phase + i * 0.9f));
        const float h = juce::jmax(3.0f, bars.getHeight() * wave);
        auto bar = juce::Rectangle<float>(bars.getX() + i * (bw + gap),
                                          bars.getCentreY() - h * 0.5f, bw, h);
        g.setColour((i % 2 == 0 ? Theme::accentViolet : Theme::accentCoral).withAlpha(0.35f + 0.5f * display));
        g.fillRoundedRectangle(bar, 1.6f);
    }
}

GenerateButton::GenerateButton() : juce::Button("Generate") {}

void GenerateButton::setBusy(bool shouldBusy)
{
    if (busy == shouldBusy)
        return;
    busy = shouldBusy;
    repaint();
}

void GenerateButton::tick()
{
    const float previousHover = hover;
    tickToward(hover, isMouseOver() || isDown() ? 1.0f : 0.0f, 0.28f);
    if (busy)
        pulse += 0.13f;
    if (busy || std::abs(hover - previousHover) > 0.01f)
        repaint();
}

void GenerateButton::paintButton(juce::Graphics& g, bool, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const float glow = busy ? 0.55f + 0.45f * (0.5f + 0.5f * std::sin(pulse)) : 0.35f + 0.25f * hover;
    g.setColour(Theme::accentViolet.withAlpha(0.16f * glow));
    g.fillRoundedRectangle(bounds.expanded(4.0f, 3.0f), 16.0f);
    Theme::fillAccentGradient(g, bounds, down ? 0.86f : (0.92f + 0.08f * hover));
    g.setColour(juce::Colours::white.withAlpha(0.16f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, 1.0f);
    g.setFont(Theme::medium(16.5f));
    g.setColour(juce::Colours::white);
    g.drawText(busy ? "Generating" : "Generate", getLocalBounds(), juce::Justification::centred, false);
}

InspireButton::InspireButton() : juce::Button("Inspire") {}

void InspireButton::setBusy(bool shouldBusy)
{
    if (busy == shouldBusy)
        return;
    busy = shouldBusy;
    repaint();
}

void InspireButton::tick()
{
    const float previousHover = hover;
    tickToward(hover, isMouseOver() || isDown() ? 1.0f : 0.0f, 0.28f);
    if (busy)
        pulse += 0.13f;
    if (busy || std::abs(hover - previousHover) > 0.01f)
        repaint();
}

void InspireButton::paintButton(juce::Graphics& g, bool, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    const float glow = busy ? 0.45f + 0.4f * (0.5f + 0.5f * std::sin(pulse)) : 0.22f + 0.28f * hover;
    g.setColour(Theme::accentCoral.withAlpha(0.14f * glow));
    g.fillRoundedRectangle(bounds.expanded(3.0f, 2.0f), 16.0f);
    Theme::fillGlass(g, bounds, 12.0f);
    g.setColour(Theme::accentViolet.interpolatedWith(Theme::accentCoral, 0.45f).withAlpha(0.55f + 0.35f * hover));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 12.0f, down ? 1.8f : 1.2f);
    auto icon = bounds.removeFromLeft(36.0f).reduced(8.0f, 12.0f);
    drawIcon(g, IconKind::Dice, icon, juce::Colours::white);
    g.setFont(Theme::medium(15.5f));
    g.setColour(juce::Colours::white);
    g.drawText(busy ? "Rolling" : "Inspire", bounds.withTrimmedRight(8.0f), juce::Justification::centred, false);
}

MoodChip::MoodChip(InspireMood m) : juce::Button(moodName(m)), mood(m)
{
    setClickingTogglesState(false);
}

void MoodChip::tick()
{
    const float previous = hover;
    tickToward(hover, isMouseOver() || getToggleState() ? 1.0f : 0.0f);
    if (std::abs(hover - previous) > 0.01f)
        repaint();
}

void MoodChip::paintButton(juce::Graphics& g, bool, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool on = getToggleState();
    if (on)
        Theme::fillAccentGradient(g, bounds, down ? 0.88f : 1.0f);
    else
    {
        g.setColour(Theme::panel.brighter(0.10f * hover));
        g.fillRoundedRectangle(bounds, 13.0f);
        g.setColour(juce::Colours::white.withAlpha(0.08f + 0.10f * hover));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 13.0f, 1.0f);
    }
    g.setFont(Theme::medium(12.0f));
    g.setColour(on ? juce::Colours::white : Theme::label);
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred, false);
}

void CaptionPill::setText(juce::String t)
{
    if (text == t)
        return;
    text = std::move(t);
    repaint();
}

void CaptionPill::setLocked(bool shouldLock)
{
    if (locked == shouldLock)
        return;
    locked = shouldLock;
    repaint();
}

void CaptionPill::tick()
{
    const float previous = hover;
    tickToward(hover, hovered ? 1.0f : 0.0f);
    if (std::abs(hover - previous) > 0.01f)
        repaint();
}

void CaptionPill::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    Theme::fillGlass(g, bounds, 11.0f);
    if (locked)
    {
        g.setColour(Theme::accentViolet.withAlpha(0.22f + 0.12f * hover));
        g.fillRoundedRectangle(bounds, 11.0f);
        g.setColour(Theme::accentViolet.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 11.0f, 1.0f);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.06f + 0.08f * hover));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 11.0f, 1.0f);
    }
    auto textArea = getLocalBounds().reduced(8, 0);
    if (locked)
    {
        auto lock = textArea.removeFromRight(14).toFloat().reduced(1.0f, 6.0f);
        drawIcon(g, IconKind::Lock, lock, Theme::accentHover);
    }
    g.setFont(Theme::medium(11.5f));
    g.setColour(Theme::label);
    g.drawText(text, textArea, juce::Justification::centred, false);
}

void CaptionPill::mouseUp(const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onClick)
        onClick();
}

void CaptionPill::mouseEnter(const juce::MouseEvent&) { hovered = true; repaint(); }
void CaptionPill::mouseExit(const juce::MouseEvent&) { hovered = false; repaint(); }

void ParamPill::setText(juce::String t)
{
    if (text == t)
        return;
    text = std::move(t);
    repaint();
}

void ParamPill::tick()
{
    const float previous = hover;
    tickToward(hover, hovered ? 1.0f : 0.0f);
    if (std::abs(hover - previous) > 0.01f)
        repaint();
}

void ParamPill::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    Theme::fillGlass(g, bounds, 14.0f);
    g.setColour(Theme::accentViolet.withAlpha(0.10f * hover));
    g.fillRoundedRectangle(bounds, 14.0f);
    g.setColour(Theme::label);
    g.setFont(Theme::medium(12.5f));
    g.drawText(text, getLocalBounds().reduced(10, 0), juce::Justification::centred, false);
}

void ParamPill::mouseUp(const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onClick)
        onClick();
}

void ParamPill::mouseEnter(const juce::MouseEvent&) { hovered = true; repaint(); }
void ParamPill::mouseExit(const juce::MouseEvent&) { hovered = false; repaint(); }

void DragHandle::setReady(bool r)
{
    ready = r;
    setMouseCursor(ready ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void DragHandle::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(ready ? (hovered ? Theme::panel.brighter(0.10f) : Theme::panel) : Theme::clipBackground);
    g.fillRoundedRectangle(bounds, 8.0f);
    drawIcon(g, IconKind::Drag, bounds, ready ? Theme::accent : Theme::outline);
}

void DragHandle::mouseDown(const juce::MouseEvent&) { dragging = false; }

void DragHandle::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging || !ready || event.getDistanceFromDragStart() < 4)
        return;
    dragging = true;
    if (onDrag)
        onDrag();
}

void DragHandle::mouseEnter(const juce::MouseEvent&) { hovered = true; repaint(); }
void DragHandle::mouseExit(const juce::MouseEvent&) { hovered = false; repaint(); }

void LabeledDrag::setReady(bool r)
{
    ready = r;
    setMouseCursor(ready ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void LabeledDrag::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(ready ? (hovered ? Theme::panel.brighter(0.12f) : Theme::panel) : Theme::clipBackground);
    g.fillRoundedRectangle(bounds, 10.0f);
    g.setColour(Theme::outline);
    g.drawRoundedRectangle(bounds, 10.0f, 1.0f);
    auto icon = getLocalBounds().removeFromLeft(36).toFloat();
    drawIcon(g, IconKind::Drag, icon, ready ? Theme::accent : Theme::outline);
    g.setColour(ready ? Theme::label : Theme::dimText);
    g.setFont(Theme::medium(13.5f));
    g.drawText("Drag all", getLocalBounds().withTrimmedLeft(32), juce::Justification::centredLeft, false);
}

void LabeledDrag::mouseDown(const juce::MouseEvent&) { dragging = false; }

void LabeledDrag::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging || !ready || event.getDistanceFromDragStart() < 4)
        return;
    dragging = true;
    if (onDrag)
        onDrag();
}

void LabeledDrag::mouseEnter(const juce::MouseEvent&) { hovered = true; repaint(); }
void LabeledDrag::mouseExit(const juce::MouseEvent&) { hovered = false; repaint(); }

TrackLanes::TrackLanes()
{
    const Part parts[] = { Part::Chords, Part::Bass, Part::Synth, Part::Drums };
    for (auto part : parts)
    {
        auto lane = std::make_unique<Lane>();
        lane->part = part;
        lane->name = laneName(part);
        lane->colour = colourForPart(part);
        lane->mute.setClickingTogglesState(true);
        addAndMakeVisible(lane->mute);
        addAndMakeVisible(lane->regen);
        addAndMakeVisible(lane->drag);
        lane->mute.onClick = [this, part]() { if (onMute) onMute(part); };
        lane->regen.onClick = [this, part]() { if (onRegen) onRegen(part); };
        lane->drag.onDrag = [this, part]() { if (onDrag) onDrag(part); };
        lanes.push_back(std::move(lane));
    }
    setReady(false);
}

void TrackLanes::setClip(const juce::MidiBuffer& midi, double lengthInBeats,
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

    hasMidi = !notes.empty();
    setReady(hasMidi);
    repaint();
}

void TrackLanes::setPlaybackBeat(double beat)
{
    if (std::abs(beat - playbackBeat) > 1.0e-4)
    {
        playbackBeat = beat;
        repaint();
    }
}

void TrackLanes::setMuteMask(int mask)
{
    if (mask == muteMask)
        return;
    muteMask = mask;
    for (auto& lane : lanes)
    {
        const bool muted = (mask & partLockBit(lane->part)) != 0;
        lane->mute.setToggleState(muted, juce::dontSendNotification);
        lane->mute.setKind(muted ? IconKind::Mute : IconKind::Unmute);
    }
    repaint();
}

void TrackLanes::setReady(bool midiReady)
{
    hasMidi = midiReady;
    for (auto& lane : lanes)
    {
        lane->drag.setReady(midiReady);
        lane->mute.setVisible(midiReady);
        lane->regen.setVisible(midiReady);
        lane->drag.setVisible(midiReady);
    }
}

void TrackLanes::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    const int laneH = area.getHeight() / juce::jmax(1, (int) lanes.size());
    for (auto& lane : lanes)
    {
        auto row = area.removeFromTop(laneH).reduced(0, 4);
        row.removeFromLeft(72);
        lane->drag.setBounds(row.removeFromRight(36).reduced(2));
        row.removeFromRight(6);
        lane->regen.setBounds(row.removeFromRight(36).reduced(2));
        row.removeFromRight(6);
        lane->mute.setBounds(row.removeFromRight(36).reduced(2));
    }
}

void TrackLanes::paintLaneRoll(juce::Graphics& g, const Lane& lane, juce::Rectangle<float> area) const
{
    g.setColour(Theme::clipBackground.withAlpha(0.92f));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(lane.colour.withAlpha(0.08f));
    g.drawRoundedRectangle(area.reduced(0.5f), 10.0f, 1.0f);

    const bool muted = (muteMask & partLockBit(lane.part)) != 0;
    if (clipLength <= 0.0)
        return;

    const int ch = partChannel(lane.part);
    int lo = 127, hi = 0;
    for (const auto& n : notes)
    {
        if (n.channel != ch)
            continue;
        lo = juce::jmin(lo, n.pitch);
        hi = juce::jmax(hi, n.pitch);
    }
    if (lo > hi)
    {
        lo = 48;
        hi = 72;
    }

    const float ppb = area.getWidth() / (float) clipLength;
    const int rows = juce::jmax(1, hi - lo + 3);
    const float rh = area.getHeight() / (float) rows;

    g.setColour(juce::Colours::white.withAlpha(0.035f));
    for (int beat = 0; beat <= (int) clipLength; ++beat)
    {
        const float x = area.getX() + beat * ppb;
        g.drawVerticalLine(juce::roundToInt(x), area.getY() + 2.0f, area.getBottom() - 2.0f);
    }

    for (const auto& n : notes)
    {
        if (n.channel != ch)
            continue;
        const float x = area.getX() + (float) n.start * ppb;
        const float w = juce::jmax(3.0f, (float) n.length * ppb - 1.0f);
        const float y = area.getY() + (hi + 1 - n.pitch) * rh;
        const bool active = playbackBeat >= n.start && playbackBeat < n.start + n.length;
        const float vel = juce::jmap((float) n.velocity, 0.0f, 127.0f, 0.42f, 1.0f);
        const auto colour = lane.colour.withAlpha(muted ? 0.16f : vel);
        auto pill = juce::Rectangle<float>(x, y + 0.6f, w, juce::jmax(2.4f, rh - 1.4f));
        if (active && !muted)
            pill = pill.expanded(0.0f, 0.8f);
        drawGlowPill(g, pill, colour, muted ? 0.0f : (active ? 1.0f : 0.35f + 0.4f * vel));
    }

    if (playbackBeat >= 0.0)
    {
        const float x = area.getX() + (float) playbackBeat * ppb;
        g.setColour(Theme::accentCoral.withAlpha(0.10f));
        g.fillRoundedRectangle(x - 7.0f, area.getY(), 14.0f, area.getHeight(), 4.0f);
        g.setColour(Theme::accentViolet.withAlpha(0.55f));
        g.fillRect(x - 1.4f, area.getY() + 1.0f, 2.8f, area.getHeight() - 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.fillRect(x - 0.6f, area.getY() + 1.0f, 1.2f, area.getHeight() - 2.0f);
    }
}

void TrackLanes::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    Theme::fillGlass(g, bounds, 16.0f);

    if (!hasMidi)
    {
        g.setColour(Theme::label);
        g.setFont(Theme::medium(16.0f));
        g.drawText("Describe a track or pick a genre, then Generate.",
                   getLocalBounds().translated(0, -10), juce::Justification::centred);
        g.setColour(Theme::dimText);
        g.setFont(Theme::regular(13.0f));
        g.drawText("Drop a .mid here to accompany, extend, rewrite or restyle.",
                   getLocalBounds().translated(0, 12), juce::Justification::centred);
        return;
    }

    auto area = getLocalBounds().reduced(12, 10);
    const int laneH = area.getHeight() / juce::jmax(1, (int) lanes.size());
    g.setFont(Theme::medium(12.5f));
    for (const auto& lane : lanes)
    {
        auto row = area.removeFromTop(laneH).reduced(0, 4);
        auto name = row.removeFromLeft(72);
        g.setColour(lane->colour.withAlpha(0.28f));
        g.fillEllipse((float) name.getX() + 1.0f, (float) name.getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour(lane->colour);
        g.fillEllipse((float) name.getX() + 3.5f, (float) name.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setColour(Theme::label);
        g.drawText(lane->name, name.withTrimmedLeft(18), juce::Justification::centredLeft, false);
        row.removeFromRight(36 + 6 + 36 + 6 + 36);
        paintLaneRoll(g, *lane, row.toFloat());
    }
}

ImportModePicker::ImportModePicker()
{
    title.setText("Use this MIDI as", juce::dontSendNotification);
    title.setFont(Theme::medium(18.0f));
    title.setColour(juce::Label::textColourId, Theme::label);
    title.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title);

    subtitle.setText("Choose how the generator should treat the file.", juce::dontSendNotification);
    subtitle.setFont(Theme::regular(13.5f));
    subtitle.setColour(juce::Label::textColourId, Theme::dimText);
    subtitle.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitle);

    auto bind = [this](juce::TextButton& b, ImportMode mode)
    {
        b.setColour(juce::TextButton::buttonColourId, Theme::panel);
        b.onClick = [this, mode]() { if (onPick) onPick(mode); };
        addAndMakeVisible(b);
    };
    bind(accompany, ImportMode::Accompany);
    bind(extend, ImportMode::Extend);
    bind(rewrite, ImportMode::Rewrite);
    bind(restyle, ImportMode::Restyle);
    cancel.setColour(juce::TextButton::buttonColourId, Theme::clipBackground);
    cancel.onClick = [this]() { if (onCancel) onCancel(); };
    addAndMakeVisible(cancel);
}

void ImportModePicker::setFileName(const juce::String& name)
{
    subtitle.setText(name.isEmpty()
                         ? "Choose how the generator should treat the file."
                         : name + "  ·  pick a mode, then we import.",
                     juce::dontSendNotification);
}

void ImportModePicker::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.fillRect(getLocalBounds());
    Theme::fillGlass(g, getLocalBounds().withSizeKeepingCentre(420, 280).toFloat(), 18.0f);
}

void ImportModePicker::resized()
{
    auto card = getLocalBounds().withSizeKeepingCentre(420, 280).reduced(24);
    title.setBounds(card.removeFromTop(28));
    card.removeFromTop(6);
    subtitle.setBounds(card.removeFromTop(36));
    card.removeFromTop(16);
    auto row = card.removeFromTop(40);
    accompany.setBounds(row.removeFromLeft((row.getWidth() - 10) / 2));
    row.removeFromLeft(10);
    extend.setBounds(row);
    card.removeFromTop(10);
    row = card.removeFromTop(40);
    rewrite.setBounds(row.removeFromLeft((row.getWidth() - 10) / 2));
    row.removeFromLeft(10);
    restyle.setBounds(row);
    card.removeFromTop(16);
    cancel.setBounds(card.removeFromTop(36).withSizeKeepingCentre(120, 36));
}

SynthesiaAIEditor::SynthesiaAIEditor(SynthesiaAIProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(wordmark);

    licenseBadge.setFont(Theme::medium(11.5f));
    licenseBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(licenseBadge);

    keyPill.onClick = [this]() { showKeyMenu(); };
    tempoPill.onClick = [this]() { showTempoMenu(); };
    barsPill.onClick = [this]() { showBarsMenu(); };
    addAndMakeVisible(keyPill);
    addAndMakeVisible(tempoPill);
    addAndMakeVisible(barsPill);

    settingsButton.onClick = [this]() { openSettings(); };
    addAndMakeVisible(settingsButton);

    promptEditor.setMultiLine(true);
    promptEditor.setReturnKeyStartsNewLine(true);
    promptEditor.setFont(Theme::regular(16.0f));
    promptEditor.setTextToShowWhenEmpty("Describe your track  ·  optional", Theme::dimText);
    promptEditor.setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground.withAlpha(0.55f));
    addAndMakeVisible(promptEditor);
    addAndMakeVisible(previewOrb);

    for (int i = 0; i < numGenres; ++i)
    {
        auto* chip = genreChips.add(new GenreChip(static_cast<Genre>(i)));
        chip->onClick = [this, i]()
        {
            inspireLocks.genre = true;
            captionGenre.setLocked(true);
            chooseGenre(static_cast<Genre>(i));
        };
        addAndMakeVisible(chip);
    }

    for (int i = 0; i < numMoods; ++i)
    {
        const auto mood = static_cast<InspireMood>(i);
        auto* chip = moodChips.add(new MoodChip(mood));
        chip->onClick = [this, chip]()
        {
            const bool already = chip->getToggleState();
            inspireLocks.mood = !already;
            captionMood.setLocked(inspireLocks.mood);
            for (auto* other : moodChips)
                other->setToggleState(false, juce::dontSendNotification);
            if (inspireLocks.mood)
                chip->setToggleState(true, juce::dontSendNotification);
        };
        addAndMakeVisible(chip);
    }

    generateButton.onClick = [this]()
    {
        if (!audioProcessor.isGenerating())
            onGenerateClicked();
    };
    addAndMakeVisible(generateButton);

    inspireButton.onClick = [this]()
    {
        if (!audioProcessor.isGenerating())
            onInspireClicked();
    };
    addAndMakeVisible(inspireButton);

    captionGenre.onClick = [this]()
    {
        inspireLocks.genre = !inspireLocks.genre;
        captionGenre.setLocked(inspireLocks.genre);
    };
    captionKey.onClick = [this]()
    {
        inspireLocks.key = !inspireLocks.key;
        captionKey.setLocked(inspireLocks.key);
    };
    captionTempo.onClick = [this]()
    {
        inspireLocks.tempo = !inspireLocks.tempo;
        captionTempo.setLocked(inspireLocks.tempo);
        if (inspireLocks.tempo)
        {
            followHostTempoButton.setToggleState(false, juce::dontSendNotification);
            audioProcessor.setFollowHostTempo(false);
        }
    };
    captionMood.onClick = [this]()
    {
        inspireLocks.mood = !inspireLocks.mood;
        captionMood.setLocked(inspireLocks.mood);
        if (!inspireLocks.mood)
            refreshMoodChips();
    };
    addAndMakeVisible(captionGenre);
    addAndMakeVisible(captionKey);
    addAndMakeVisible(captionTempo);
    addAndMakeVisible(captionMood);

    inspireBack.onClick = [this]() { showInspireHistory(inspireIndex - 1); };
    inspireForward.onClick = [this]() { showInspireHistory(inspireIndex + 1); };
    addAndMakeVisible(inspireBack);
    addAndMakeVisible(inspireForward);

    againButton.setButtonText("Again");
    againButton.setColour(juce::TextButton::buttonColourId, Theme::panel.brighter(0.06f));
    againButton.setColour(juce::TextButton::textColourOffId, Theme::label);
    againButton.setTooltip("Roll another idea (keeps locked pills)");
    againButton.onClick = [this]()
    {
        if (!audioProcessor.isGenerating())
            onInspireAgain();
    };
    addAndMakeVisible(againButton);
    inspireBack.setVisible(false);
    inspireForward.setVisible(false);
    againButton.setVisible(false);
    captionGenre.setVisible(false);
    captionKey.setVisible(false);
    captionTempo.setVisible(false);
    captionMood.setVisible(false);

    importButton.setButtonText("Import MIDI");
    importButton.onClick = [this]() { onImportClicked(); };
    addAndMakeVisible(importButton);

    moreButton.setButtonText("More options");
    moreButton.onClick = [this]() { setMoreOpen(!moreOpen); };
    addAndMakeVisible(moreButton);

    setupSectionLabel(moreArrangement, "ARRANGEMENT");
    setupSectionLabel(moreHarmony, "HARMONY");
    setupSectionLabel(moreFeel, "FEEL");
    setupSectionLabel(moreStyle, "STYLE");
    setupSectionLabel(moreLibrary, "LIBRARY");
    morePage.addAndMakeVisible(moreArrangement);
    morePage.addAndMakeVisible(moreHarmony);
    morePage.addAndMakeVisible(moreFeel);
    morePage.addAndMakeVisible(moreStyle);
    morePage.addAndMakeVisible(moreLibrary);

    trackLanes.onMute = [this](Part part)
    {
        audioProcessor.setPartMuted(part, !audioProcessor.isPartMuted(part));
        trackLanes.setMuteMask(audioProcessor.getMuteMask());
    };
    trackLanes.onRegen = [this](Part part) { onRegeneratePart(part, false); };
    trackLanes.onDrag = [this](Part part) { startExternalMidiDrag(part); };
    addAndMakeVisible(trackLanes);

    dragAllButton.onDrag = [this]() { startExternalMidiDrag(Part::All); };
    addAndMakeVisible(dragAllButton);

    previewButton.onClick = [this]()
    {
        audioProcessor.setPreviewPlaying(!audioProcessor.isPreviewPlaying());
    };
    addAndMakeVisible(previewButton);

    statusLabel.setColour(juce::Label::textColourId, Theme::dimText);
    statusLabel.setFont(Theme::regular(13.0f));
    addAndMakeVisible(statusLabel);

    moreViewport.setViewedComponent(&morePage, false);
    moreViewport.setScrollBarsShown(true, false);
    moreViewport.setScrollBarThickness(7);
    addChildComponent(moreViewport);

    auto addMore = [this](juce::Component& c) { morePage.addAndMakeVisible(c); };
    setupLabel(scaleLabel, "Scale");
    addMore(scaleLabel);
    for (int i = 0; i < 5; ++i)
        scaleComboBox.addItem(scaleNames[i], i + 1);
    scaleComboBox.setSelectedId(1, juce::dontSendNotification);
    addMore(scaleComboBox);

    setupLabel(formLabel, "Form");
    addMore(formLabel);
    formComboBox.addItem("Loop", 1);
    formComboBox.addItem("Song form", 2);
    formComboBox.setSelectedId(1, juce::dontSendNotification);
    addMore(formComboBox);

    setupLabel(gridLabel, "Grid");
    addMore(gridLabel);
    gridComboBox.addItem("1/16", 1);
    gridComboBox.addItem("1/8", 2);
    gridComboBox.addItem("1/4", 3);
    gridComboBox.addItem("1/2", 4);
    gridComboBox.setSelectedId(2, juce::dontSendNotification);
    addMore(gridComboBox);

    setupLabel(engineLabel, "Engine");
    addMore(engineLabel);
    engineComboBox.addItem("Auto (prompt → LLM)", 1);
    engineComboBox.addItem("Genre engine", 2);
    engineComboBox.addItem("Markov", 3);
    engineComboBox.addItem("LLM planner", 4);
    engineComboBox.setSelectedId(1, juce::dontSendNotification);
    engineComboBox.onChange = [this]() { applyEngineChoice(); snapshotParams(); };
    addMore(engineComboBox);

    setupLabel(partLabel, "Generate");
    addMore(partLabel);
    partComboBox.addItem("All parts", 1);
    for (auto part : allParts)
        partComboBox.addItem(laneName(part), static_cast<int>(part) + 1);
    partComboBox.setSelectedId(1, juce::dontSendNotification);
    addMore(partComboBox);

    setupLabel(chordsLabel, "Chords");
    addMore(chordsLabel);
    chordsEditor.setTextToShowWhenEmpty("Am9 Fmaj9 Dm7 E7b9", Theme::dimText);
    addMore(chordsEditor);
    lockProgressionButton.setButtonText("Lock chords");
    addMore(lockProgressionButton);

    setupLabel(locksLabel, "Keep");
    addMore(locksLabel);
    for (auto part : allParts)
    {
        auto* button = partLockButtons.add(new juce::ToggleButton(laneName(part)));
        addMore(*button);
    }

    planButton.setButtonText("Plan arrangement");
    planButton.onClick = [this]() { onPlanClicked(); };
    addMore(planButton);
    planSummaryEditor.setMultiLine(true);
    planSummaryEditor.setReadOnly(true);
    planSummaryEditor.setFont(Theme::regular(12.5f));
    addMore(planSummaryEditor);

    setupLabel(pianoLabel, "Piano");
    addMore(pianoLabel);
    pianoMute.setClickingTogglesState(true);
    pianoMute.onClick = [this]()
    {
        audioProcessor.setPartMuted(Part::Piano, pianoMute.getToggleState());
        pianoMute.setKind(pianoMute.getToggleState() ? IconKind::Mute : IconKind::Unmute);
    };
    pianoRegen.onClick = [this]() { onRegeneratePart(Part::Piano, false); };
    pianoDrag.onDrag = [this]() { startExternalMidiDrag(Part::Piano); };
    addMore(pianoMute);
    addMore(pianoRegen);
    addMore(pianoDrag);

    setupLabel(seedLabel, "Seed");
    addMore(seedLabel);
    seedSlider.setRange(0, 9999, 1);
    seedSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    styleSlider(seedSlider);
    addMore(seedSlider);

    setupLabel(swingLabel, "Swing");
    addMore(swingLabel);
    swingSlider.setRange(0.50, 0.72, 0.01);
    swingSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 22);
    styleSlider(swingSlider);
    addMore(swingSlider);

    setupLabel(humanizeLabel, "Humanize");
    addMore(humanizeLabel);
    humanizeSlider.setRange(0, 16, 0.5);
    humanizeSlider.setTextValueSuffix(" ms");
    humanizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 22);
    styleSlider(humanizeSlider);
    addMore(humanizeSlider);

    setupLabel(densityLabel, "Density");
    addMore(densityLabel);
    densitySlider.setRange(0, 100, 1);
    densitySlider.setTextValueSuffix("%");
    densitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 22);
    styleSlider(densitySlider);
    addMore(densitySlider);

    setupLabel(importModeLabel, "Import as");
    addMore(importModeLabel);
    importModeComboBox.addItem("Off", 1);
    importModeComboBox.addItem("Accompany", 2);
    importModeComboBox.addItem("Extend", 3);
    importModeComboBox.addItem("Rewrite", 4);
    importModeComboBox.addItem("Restyle", 5);
    importModeComboBox.setSelectedId(1, juce::dontSendNotification);
    addMore(importModeComboBox);

    followHostTempoButton.setButtonText("Follow host tempo");
    followHostTempoButton.setToggleState(audioProcessor.getFollowHostTempo(), juce::dontSendNotification);
    followHostTempoButton.onClick = [this]()
    {
        audioProcessor.setFollowHostTempo(followHostTempoButton.getToggleState());
        refreshPills();
    };
    addMore(followHostTempoButton);

    followHostKeyButton.setButtonText("Keep my key");
    followHostKeyButton.setToggleState(audioProcessor.getFollowHostKey(), juce::dontSendNotification);
    followHostKeyButton.onClick = [this]()
    {
        audioProcessor.setFollowHostKey(followHostKeyButton.getToggleState());
    };
    addMore(followHostKeyButton);

    setupLabel(styleProfileLabel, "Style");
    addMore(styleProfileLabel);
    styleProfileComboBox.onChange = [this]()
    {
        if (styleProfileComboBox.getSelectedId() <= 1)
            audioProcessor.setCurrentStyle({});
        else
            audioProcessor.setCurrentStyle(styleProfileComboBox.getText());
        snapshotParams();
    };
    addMore(styleProfileComboBox);

    setupLabel(blendLabel, "Blend");
    addMore(blendLabel);
    blendSlider.setRange(0, 100, 1);
    blendSlider.setTextValueSuffix("%");
    blendSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 22);
    styleSlider(blendSlider);
    blendSlider.onValueChange = [this]()
    {
        audioProcessor.setStyleBlend(blendSlider.getValue() / 100.0);
        snapshotParams();
    };
    addMore(blendSlider);

    learnStyleButton.setButtonText("Learn style...");
    learnStyleButton.onClick = [this]() { onLearnStyleClicked(); };
    addMore(learnStyleButton);

    captureButton.setButtonText("Capture MIDI");
    captureButton.onClick = [this]() { onCaptureClicked(); };
    addMore(captureButton);

    exportButton.setButtonText("Export MIDI");
    exportButton.onClick = [this]() { onExportMidi(); };
    addMore(exportButton);

    setupLabel(presetLabel, "Preset");
    addMore(presetLabel);
    presetComboBox.onChange = [this]() { applyPresetFromCombo(); };
    addMore(presetComboBox);
    savePresetButton.setButtonText("Save");
    savePresetButton.onClick = [this]() { onSavePresetClicked(); };
    addMore(savePresetButton);

    setupLabel(varyLabel, "Variation");
    addMore(varyLabel);
    for (auto part : allParts)
    {
        auto* button = variationButtons.add(new juce::TextButton(juce::String("Vary ") + laneName(part)));
        button->onClick = [this, part]() { onRegeneratePart(part, true); };
        addMore(*button);
    }

    importPicker.onPick = [this](ImportMode mode) { applyImportMode(mode); };
    importPicker.onCancel = [this]() { hideImportPicker(); };
    addChildComponent(importPicker);

    settingsOverlay = std::make_unique<SettingsOverlay>(audioProcessor);
    settingsOverlay->onClose = [this]() { hideSettings(); };
    addChildComponent(*settingsOverlay);

    refreshStyleCombo();
    refreshPresetCombo();
    applyParamsToUI(audioProcessor.getLastUiParams());
    chooseGenre(audioProcessor.getLastUiParams().genre);
    refreshPills();

    setResizable(true, true);
    setResizeLimits(680, 640, 1280, 1100);
    setSize(800, 840);
    rebuildBackdrop();
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

void SynthesiaAIEditor::setupSectionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, Theme::dimText);
    label.setFont(Theme::medium(11.0f));
}

void SynthesiaAIEditor::chooseGenre(Genre genre)
{
    genreValue = genre;
    for (int i = 0; i < genreChips.size(); ++i)
        genreChips[i]->setToggleState(i == static_cast<int>(genre), juce::dontSendNotification);
    if (!applyingPreset)
        applyGenreDefaults();
}

Genre SynthesiaAIEditor::selectedGenre() const { return genreValue; }
ScaleType SynthesiaAIEditor::selectedScale() const
{
    return static_cast<ScaleType>(juce::jmax(0, scaleComboBox.getSelectedId() - 1));
}

void SynthesiaAIEditor::applyGenreDefaults()
{
    if (applyingPreset)
        return;
    const auto& profile = getGenreProfile(genreValue);
    if (!followHostTempoButton.getToggleState())
        tempoValue = profile.defaultTempo;
    gridComboBox.setSelectedId(static_cast<int>(profile.defaultGrid) + 1, juce::dontSendNotification);
    audioProcessor.loadGenreSounds(genreValue);
    refreshPills();
}

void SynthesiaAIEditor::refreshPills()
{
    const auto scale = selectedScale();
    keyPill.setText(juce::String(keyNames[juce::jlimit(0, 11, keyIndex)]) + "  "
                    + juce::String(scaleNames[static_cast<int>(scale)]).substring(0, 3));
    double bpm = tempoValue;
    juce::String tempoText;
    if (followHostTempoButton.getToggleState() && audioProcessor.getHostTempo() > 20.0)
    {
        bpm = audioProcessor.getHostTempo();
        tempoText = juce::String(juce::roundToInt(bpm)) + " bpm  host";
    }
    else
    {
        tempoText = juce::String(juce::roundToInt(bpm)) + " bpm";
    }
    tempoPill.setText(tempoText);
    barsPill.setText(juce::String(barsValue) + " bars");
}

void SynthesiaAIEditor::showKeyMenu()
{
    juce::PopupMenu menu;
    for (int i = 0; i < 12; ++i)
        menu.addItem(i + 1, keyNames[i], true, i == keyIndex);
    menu.addSeparator();
    for (int i = 0; i < 5; ++i)
        menu.addItem(20 + i, scaleNames[i], true, scaleComboBox.getSelectedId() == i + 1);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&keyPill),
                       [this](int result)
                       {
                           if (result >= 1 && result <= 12)
                               keyIndex = result - 1;
                           else if (result >= 20 && result <= 24)
                               scaleComboBox.setSelectedId(result - 19, juce::dontSendNotification);
                           refreshPills();
                           snapshotParams();
                       });
}

void SynthesiaAIEditor::showTempoMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Follow host", true, followHostTempoButton.getToggleState());
    menu.addSeparator();
    static const int tempoChoices[] = { 80, 88, 110, 120, 124, 140 };
    for (int i = 0; i < 6; ++i)
        menu.addItem(10 + i, juce::String(tempoChoices[i]) + " bpm");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&tempoPill),
                       [this](int result)
                       {
                           if (result == 1)
                           {
                               followHostTempoButton.setToggleState(!followHostTempoButton.getToggleState(),
                                                                    juce::sendNotification);
                               return;
                           }
                           if (result >= 10)
                           {
                               static const int chosenTempo[] = { 80, 88, 110, 120, 124, 140 };
                               followHostTempoButton.setToggleState(false, juce::dontSendNotification);
                               audioProcessor.setFollowHostTempo(false);
                               tempoValue = chosenTempo[result - 10];
                               refreshPills();
                               snapshotParams();
                           }
                       });
}

void SynthesiaAIEditor::showBarsMenu()
{
    juce::PopupMenu menu;
    const int bars[] = { 4, 8, 16, 24, 32 };
    for (int i = 0; i < 5; ++i)
        menu.addItem(bars[i], juce::String(bars[i]) + " bars", true, barsValue == bars[i]);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&barsPill),
                       [this](int result)
                       {
                           if (result > 0)
                           {
                               barsValue = result;
                               refreshPills();
                               snapshotParams();
                           }
                       });
}

void SynthesiaAIEditor::setMoreOpen(bool open)
{
    moreOpen = open;
    moreButton.setButtonText(open ? "Hide options" : "More options");
    moreViewport.setVisible(open);
    resized();
}

void SynthesiaAIEditor::showImportPicker(const juce::File& file)
{
    hideSettings();
    pendingImportFile = file;
    importPicker.setFileName(file.getFileName());
    importPicker.setVisible(true);
    importPickerOpen = true;
    importPicker.toFront(true);
    resized();
}

void SynthesiaAIEditor::hideImportPicker()
{
    importPicker.setVisible(false);
    importPickerOpen = false;
    pendingImportFile = juce::File();
}

void SynthesiaAIEditor::applyImportMode(ImportMode mode)
{
    switch (mode)
    {
        case ImportMode::Accompany:
            partComboBox.setSelectedId(1, juce::dontSendNotification);
            break;
        case ImportMode::Extend:
            formComboBox.setSelectedId(2, juce::dontSendNotification);
            if (barsValue < 16)
                barsValue = 24;
            break;
        case ImportMode::Rewrite:
        case ImportMode::Restyle:
        case ImportMode::None:
            break;
    }
    importModeComboBox.setSelectedId(static_cast<int>(mode) + 1, juce::dontSendNotification);

    const auto file = pendingImportFile;
    hideImportPicker();

    auto params = collectParams();
    params.importMode = mode;
    audioProcessor.setLastUiParams(params);

    if (file.existsAsFile())
    {
        importPath(file);
        audioProcessor.generateMidi(collectParams());
        statusLabel.setText("Generating from imported MIDI...", juce::dontSendNotification);
    }
    else
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Import MIDI file...",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.mid;*.midi");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser, mode](const juce::FileChooser& fc)
                             {
                                 const auto chosen = fc.getResult();
                                 if (!chosen.existsAsFile())
                                     return;
                                 auto imported = collectParams();
                                 imported.importMode = mode;
                                 audioProcessor.setLastUiParams(imported);
                                 if (importPath(chosen))
                                 {
                                     audioProcessor.generateMidi(collectParams());
                                     statusLabel.setText("Generating from imported MIDI...",
                                                         juce::dontSendNotification);
                                 }
                             });
    }
}

void SynthesiaAIEditor::openSettings()
{
    hideImportPicker();
    if (settingsOverlay != nullptr)
    {
        settingsOverlay->refreshFromProcessor();
        settingsOverlay->setVisible(true);
        settingsOverlay->toFront(true);
        settingsOpen = true;
        resized();
    }
}

void SynthesiaAIEditor::hideSettings()
{
    if (settingsOverlay != nullptr)
        settingsOverlay->setVisible(false);
    settingsOpen = false;
}

void SynthesiaAIEditor::applyEngineChoice()
{
    const int id = engineComboBox.getSelectedId();
    if (id == 3)
        audioProcessor.setGeneratorType(GeneratorType::Markov);
    else if (id == 4)
        audioProcessor.setGeneratorType(GeneratorType::LLM);
    else if (id == 2)
        audioProcessor.setGeneratorType(GeneratorType::Genre);
}

void SynthesiaAIEditor::rebuildBackdrop()
{
    const int w = juce::jmax(1, getWidth());
    const int h = juce::jmax(1, getHeight());
    backdrop = juce::Image(juce::Image::ARGB, w, h, true);
    juce::Graphics g(backdrop);
    g.fillAll(Theme::background);

    juce::ColourGradient violet(Theme::accentViolet.withAlpha(0.20f), 0.0f, 0.0f,
                                juce::Colours::transparentBlack, (float) w * 0.72f, (float) h * 0.58f, true);
    g.setGradientFill(violet);
    g.fillAll();
    juce::ColourGradient coral(Theme::accentCoral.withAlpha(0.14f), (float) w, (float) h,
                               juce::Colours::transparentBlack, (float) w * 0.28f, (float) h * 0.38f, true);
    g.setGradientFill(coral);
    g.fillAll();
    juce::ColourGradient teal(Theme::accentAlt.withAlpha(0.07f), (float) w * 0.85f, (float) h * 0.18f,
                              juce::Colours::transparentBlack, (float) w * 0.4f, (float) h * 0.7f, true);
    g.setGradientFill(teal);
    g.fillAll();

    juce::Random rng(0x5e11);
    for (int i = 0; i < 1400; ++i)
    {
        const int x = rng.nextInt(w);
        const int y = rng.nextInt(h);
        g.setColour(juce::Colours::white.withAlpha(0.015f + rng.nextFloat() * 0.03f));
        g.fillRect(x, y, 1, 1);
    }
}

void SynthesiaAIEditor::paint(juce::Graphics& g)
{
    if (backdrop.isValid())
        g.drawImageAt(backdrop, 0, 0);
    else
        g.fillAll(Theme::background);

    if (moreOpen && moreViewport.isVisible())
        Theme::fillGlass(g, moreViewport.getBounds().toFloat(), 14.0f);

    if (draggingFiles)
    {
        g.setColour(Theme::accentViolet.withAlpha(0.12f));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 18.0f);
        g.setColour(Theme::accentCoral);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 18.0f, 1.6f);
        g.setFont(Theme::medium(18.0f));
        g.setColour(juce::Colours::white);
        g.drawText("Drop MIDI to import", getLocalBounds(), juce::Justification::centred);
    }
}

void SynthesiaAIEditor::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    auto header = area.removeFromTop(36);
    wordmark.setBounds(header.removeFromLeft(178));
    settingsButton.setBounds(header.removeFromRight(36));
    header.removeFromRight(8);
    barsPill.setBounds(header.removeFromRight(78));
    header.removeFromRight(8);
    tempoPill.setBounds(header.removeFromRight(108));
    header.removeFromRight(8);
    keyPill.setBounds(header.removeFromRight(92));
    if (audioProcessor.isAdvancedUi())
    {
        header.removeFromRight(8);
        licenseBadge.setBounds(header.removeFromRight(64));
        licenseBadge.setVisible(true);
    }
    else
    {
        licenseBadge.setVisible(false);
        licenseBadge.setBounds({});
    }

    area.removeFromTop(16);
    auto promptRow = area.removeFromTop(72);
    previewOrb.setBounds(promptRow.removeFromRight(72).reduced(4, 4));
    promptRow.removeFromRight(10);
    promptEditor.setBounds(promptRow);
    area.removeFromTop(12);

    auto chipRow = area.removeFromTop(34);
    const int chipW = chipRow.getWidth() / juce::jmax(1, genreChips.size());
    for (auto* chip : genreChips)
        chip->setBounds(chipRow.removeFromLeft(chipW).reduced(4, 1));

    area.removeFromTop(8);
    auto moodRow = area.removeFromTop(26);
    const int moodW = moodRow.getWidth() / juce::jmax(1, moodChips.size());
    for (auto* chip : moodChips)
        chip->setBounds(moodRow.removeFromLeft(moodW).reduced(4, 1));

    area.removeFromTop(12);
    auto actionRow = area.removeFromTop(48);
    const int actionGap = 10;
    const int inspireW = juce::jmax(168, actionRow.getWidth() * 38 / 100);
    generateButton.setBounds(actionRow.removeFromLeft(actionRow.getWidth() - inspireW - actionGap));
    actionRow.removeFromLeft(actionGap);
    inspireButton.setBounds(actionRow);

    if (inspireIndex >= 0)
    {
        area.removeFromTop(8);
        auto captionRow = area.removeFromTop(26);
        inspireBack.setBounds(captionRow.removeFromLeft(28));
        captionRow.removeFromLeft(4);
        againButton.setBounds(captionRow.removeFromRight(70));
        captionRow.removeFromRight(4);
        inspireForward.setBounds(captionRow.removeFromRight(28));
        captionRow.removeFromRight(6);
        const int pillW = captionRow.getWidth() / 4;
        captionGenre.setBounds(captionRow.removeFromLeft(pillW).reduced(3, 1));
        captionKey.setBounds(captionRow.removeFromLeft(pillW).reduced(3, 1));
        captionTempo.setBounds(captionRow.removeFromLeft(pillW).reduced(3, 1));
        captionMood.setBounds(captionRow.reduced(3, 1));
    }
    else
    {
        inspireBack.setBounds({});
        inspireForward.setBounds({});
        againButton.setBounds({});
        captionGenre.setBounds({});
        captionKey.setBounds({});
        captionTempo.setBounds({});
        captionMood.setBounds({});
    }
    area.removeFromTop(10);

    auto secondary = area.removeFromTop(34);
    importButton.setBounds(secondary.removeFromLeft(130));
    secondary.removeFromLeft(10);
    moreButton.setBounds(secondary.removeFromLeft(150));

    area.removeFromTop(14);
    statusLabel.setBounds(area.removeFromBottom(20));
    area.removeFromBottom(8);
    auto transport = area.removeFromBottom(40);
    dragAllButton.setBounds(transport.removeFromLeft(120));
    transport.removeFromLeft(10);
    previewButton.setBounds(transport.removeFromLeft(40));

    area.removeFromBottom(12);
    if (moreOpen)
    {
        const int moreH = juce::jmin(260, area.getHeight() / 2);
        moreViewport.setBounds(area.removeFromBottom(moreH));
        morePage.setSize(moreViewport.getMaximumVisibleWidth() - 8, 980);
        area.removeFromBottom(10);

        auto box = morePage.getLocalBounds().reduced(8, 6);
        auto section = [&box](juce::Label& label)
        {
            label.setBounds(box.removeFromTop(18));
            box.removeFromTop(6);
        };
        auto rowOf = [&box](juce::Label& label, juce::Component& control)
        {
            auto row = box.removeFromTop(30);
            label.setBounds(row.removeFromLeft(88));
            control.setBounds(row);
            box.removeFromTop(8);
        };

        section(moreArrangement);
        rowOf(formLabel, formComboBox);
        rowOf(gridLabel, gridComboBox);
        rowOf(engineLabel, engineComboBox);
        rowOf(partLabel, partComboBox);
        planButton.setBounds(box.removeFromTop(32));
        box.removeFromTop(8);
        planSummaryEditor.setBounds(box.removeFromTop(64));
        box.removeFromTop(12);

        section(moreHarmony);
        rowOf(scaleLabel, scaleComboBox);
        {
            auto row = box.removeFromTop(30);
            chordsLabel.setBounds(row.removeFromLeft(88));
            lockProgressionButton.setBounds(row.removeFromRight(110));
            chordsEditor.setBounds(row.reduced(6, 0));
            box.removeFromTop(8);
        }
        {
            auto row = box.removeFromTop(26);
            locksLabel.setBounds(row.removeFromLeft(88));
            const int w = row.getWidth() / juce::jmax(1, partLockButtons.size());
            for (auto* b : partLockButtons)
                b->setBounds(row.removeFromLeft(w));
            box.removeFromTop(12);
        }

        section(moreFeel);
        rowOf(seedLabel, seedSlider);
        rowOf(swingLabel, swingSlider);
        rowOf(humanizeLabel, humanizeSlider);
        rowOf(densityLabel, densitySlider);
        followHostTempoButton.setBounds(box.removeFromTop(24));
        box.removeFromTop(4);
        followHostKeyButton.setBounds(box.removeFromTop(24));
        box.removeFromTop(12);

        section(moreStyle);
        rowOf(styleProfileLabel, styleProfileComboBox);
        const bool advanced = audioProcessor.isAdvancedUi();
        blendLabel.setVisible(advanced);
        blendSlider.setVisible(advanced);
        if (advanced)
            rowOf(blendLabel, blendSlider);
        learnStyleButton.setBounds(box.removeFromTop(32));
        box.removeFromTop(12);

        section(moreLibrary);
        rowOf(importModeLabel, importModeComboBox);
        {
            auto row = box.removeFromTop(32);
            pianoLabel.setBounds(row.removeFromLeft(88));
            pianoMute.setBounds(row.removeFromLeft(36));
            row.removeFromLeft(6);
            pianoRegen.setBounds(row.removeFromLeft(36));
            row.removeFromLeft(6);
            pianoDrag.setBounds(row.removeFromLeft(36));
            box.removeFromTop(8);
        }
        captureButton.setBounds(box.removeFromTop(32));
        box.removeFromTop(8);
        exportButton.setBounds(box.removeFromTop(32));
        box.removeFromTop(8);
        {
            auto row = box.removeFromTop(30);
            presetLabel.setBounds(row.removeFromLeft(88));
            savePresetButton.setBounds(row.removeFromRight(64));
            row.removeFromRight(8);
            presetComboBox.setBounds(row);
            box.removeFromTop(8);
        }
        varyLabel.setBounds(box.removeFromTop(20));
        box.removeFromTop(4);
        {
            auto row = box.removeFromTop(28);
            const int w = row.getWidth() / juce::jmax(1, variationButtons.size());
            for (auto* b : variationButtons)
                b->setBounds(row.removeFromLeft(w).reduced(2, 0));
        }
    }

    trackLanes.setBounds(area);
    importPicker.setBounds(getLocalBounds());
    if (settingsOverlay != nullptr)
        settingsOverlay->setBounds(getLocalBounds());

    if (backdrop.getWidth() != getWidth() || backdrop.getHeight() != getHeight())
        rebuildBackdrop();
}

void SynthesiaAIEditor::timerCallback()
{
    updateUIState();
    const bool busy = audioProcessor.isGenerating();
    generateButton.setBusy(busy);
    generateButton.tick();
    inspireButton.setBusy(busy);
    inspireButton.tick();
    previewOrb.setLevel(audioProcessor.getPreviewLevel());
    previewOrb.setPlaying(audioProcessor.isPreviewPlaying() || audioProcessor.getPlaybackBeat() >= 0.0);
    previewOrb.tick();
    for (auto* chip : genreChips)
        chip->tick();
    for (auto* chip : moodChips)
        chip->tick();
    captionGenre.tick();
    captionKey.tick();
    captionTempo.tick();
    captionMood.tick();
    keyPill.tick();
    tempoPill.tick();
    barsPill.tick();
}

MidiGenerationParams SynthesiaAIEditor::collectParams() const
{
    MidiGenerationParams params;
    params.rootNote = keyIndex;
    params.scale = selectedScale();
    params.lengthInBars = barsValue;
    params.quantization = static_cast<Quantization>(juce::jmax(0, gridComboBox.getSelectedId() - 1));
    params.stylePrompt = promptEditor.getText().toStdString();
    params.tempo = (followHostTempoButton.getToggleState() && audioProcessor.getHostTempo() > 20.0)
        ? audioProcessor.getHostTempo() : tempoValue;
    params.genre = genreValue;
    params.part = static_cast<Part>(juce::jmax(0, partComboBox.getSelectedId() - 1));
    params.form = formComboBox.getSelectedId() == 2 ? ArrangementForm::Song : ArrangementForm::Loop;
    params.chordText = chordsEditor.getText().toStdString();
    params.lockProgression = lockProgressionButton.getToggleState() || !params.chordText.empty();
    params.lockedParts = 0;
    for (int i = 0; i < partLockButtons.size(); ++i)
        if (partLockButtons[i]->getToggleState())
            params.lockedParts |= partLockBit(allParts[static_cast<size_t>(i)]);
    params.importMode = static_cast<ImportMode>(juce::jmax(0, importModeComboBox.getSelectedId() - 1));
    params.styleBlend = blendSlider.getValue() / 100.0;
    params.swing = swingSlider.getValue();
    params.humanizeMs = humanizeSlider.getValue();
    params.density = (float) (densitySlider.getValue() / 100.0);
    params.seed = static_cast<unsigned int>(seedSlider.getValue());
    if (inspireIndex >= 0 && inspireIndex < (int) inspireHistory.size())
        params.grooveIndex = inspireHistory[(size_t) inspireIndex].params.grooveIndex;
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
    keyIndex = juce::jlimit(0, 11, params.rootNote);
    scaleComboBox.setSelectedId(static_cast<int>(params.scale) + 1, juce::dontSendNotification);
    chooseGenre(params.genre);
    partComboBox.setSelectedId(static_cast<int>(params.part) + 1, juce::dontSendNotification);
    formComboBox.setSelectedId(params.form == ArrangementForm::Song ? 2 : 1, juce::dontSendNotification);
    barsValue = juce::jlimit(1, 32, params.lengthInBars);
    tempoValue = params.tempo;
    gridComboBox.setSelectedId(static_cast<int>(params.quantization) + 1, juce::dontSendNotification);
    promptEditor.setText(params.stylePrompt);
    chordsEditor.setText(params.chordText);
    lockProgressionButton.setToggleState(params.lockProgression, juce::dontSendNotification);
    for (int i = 0; i < partLockButtons.size(); ++i)
        partLockButtons[i]->setToggleState(isPartLocked(params.lockedParts, allParts[static_cast<size_t>(i)]),
                                           juce::dontSendNotification);
    blendSlider.setValue(params.styleBlend * 100.0, juce::dontSendNotification);
    swingSlider.setValue(params.swing > 0.0 ? params.swing : 0.5, juce::dontSendNotification);
    humanizeSlider.setValue(params.humanizeMs, juce::dontSendNotification);
    densitySlider.setValue(params.density > 0.0f ? params.density * 100.0 : 0.0, juce::dontSendNotification);
    seedSlider.setValue((double) params.seed, juce::dontSendNotification);
    importModeComboBox.setSelectedId(static_cast<int>(params.importMode) + 1, juce::dontSendNotification);
    applyingPreset = false;
    refreshPills();
}

void SynthesiaAIEditor::refreshPresetCombo()
{
    const auto current = audioProcessor.getCurrentPresetName();
    presetComboBox.clear(juce::dontSendNotification);
    int id = 1, selected = 0;
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
    if (name.isEmpty() || !audioProcessor.applyPreset(name))
        return;
    shownPresetVersion = audioProcessor.getPresetVersion();
    shownUiParamsVersion = audioProcessor.getUiParamsVersion();
    applyParamsToUI(audioProcessor.getLastUiParams());
    statusLabel.setText("Loaded " + name, juce::dontSendNotification);
}

void SynthesiaAIEditor::onSavePresetClicked()
{
    snapshotParams();
    auto* window = new juce::AlertWindow("Save preset", "Stores the current generator settings.",
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("name", audioProcessor.getCurrentPresetName(), "Name");
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, window](int result)
        {
            if (result != 1)
                return;
            const auto name = window->getTextEditorContents("name");
            if (!audioProcessor.saveUserPreset(name))
                statusLabel.setText("Could not save preset", juce::dontSendNotification);
            else
            {
                shownPresetVersion = audioProcessor.getPresetVersion();
                refreshPresetCombo();
                statusLabel.setText("Saved " + name, juce::dontSendNotification);
            }
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

void SynthesiaAIEditor::applyPlanToUI(const ArrangementPlan& plan)
{
    if (!audioProcessor.getFollowHostKey())
    {
        keyIndex = juce::jlimit(0, 11, plan.rootNote);
        scaleComboBox.setSelectedId(static_cast<int>(plan.scale) + 1, juce::dontSendNotification);
    }
    if (!followHostTempoButton.getToggleState())
        tempoValue = plan.tempo;
    barsValue = plan.lengthInBars;
    formComboBox.setSelectedId(plan.sections.size() > 1 ? 2 : 1, juce::dontSendNotification);
    chordsEditor.setText(plan.chordChart());
    lockProgressionButton.setToggleState(true, juce::dontSendNotification);
    planSummaryEditor.setText(plan.summary());
    if (!audioProcessor.getFollowHostKey())
        chooseGenre(plan.genre);
    refreshPills();
}

void SynthesiaAIEditor::applyImportToUI(const MidiAnalysis& analysis)
{
    if (!analysis.ok)
    {
        statusLabel.setText(analysis.error.isEmpty() ? "Import failed" : analysis.error,
                            juce::dontSendNotification);
        return;
    }
    keyIndex = juce::jlimit(0, 11, analysis.rootNote);
    scaleComboBox.setSelectedId(static_cast<int>(analysis.scale) + 1, juce::dontSendNotification);
    tempoValue = analysis.tempo;
    if (audioProcessor.getLastUiParams().importMode != ImportMode::Extend)
        barsValue = analysis.lengthInBars;
    chordsEditor.setText(analysis.chordChart);
    lockProgressionButton.setToggleState(true, juce::dontSendNotification);
    for (int i = 0; i < partLockButtons.size(); ++i)
        partLockButtons[i]->setToggleState(allParts[static_cast<size_t>(i)] == analysis.primaryPart,
                                           juce::dontSendNotification);
    planSummaryEditor.setText(analysis.summary);
    refreshPills();
    statusLabel.setText("Imported " + analysis.sourceName, juce::dontSendNotification);
}

void SynthesiaAIEditor::onPlanClicked()
{
    applyEngineChoice();
    audioProcessor.planArrangement(collectParams());
    statusLabel.setText("Planning...", juce::dontSendNotification);
}

void SynthesiaAIEditor::onGenerateClicked()
{
    const int engine = engineComboBox.getSelectedId();
    if (engine <= 1)
    {
        audioProcessor.setGeneratorType(promptEditor.getText().trim().isEmpty()
                                            ? GeneratorType::Genre : GeneratorType::LLM);
    }
    else
    {
        applyEngineChoice();
    }
    audioProcessor.generateMidi(collectParams());
    statusLabel.setText("Generating...", juce::dontSendNotification);
}

void SynthesiaAIEditor::onInspireClicked()
{
    InspireIdea base;
    if (inspireIndex >= 0 && inspireIndex < (int) inspireHistory.size())
        base = inspireHistory[(size_t) inspireIndex];
    else
        base.params = collectParams();

    if (inspireLocks.genre)
        base.params.genre = genreValue;
    if (inspireLocks.key)
    {
        base.params.rootNote = keyIndex;
        base.params.scale = selectedScale();
    }
    if (inspireLocks.tempo)
        base.params.tempo = tempoValue;
    if (inspireLocks.mood)
    {
        for (auto* chip : moodChips)
            if (chip->getToggleState())
                base.mood = chip->getMood();
    }

    auto idea = rollInspire(&base, inspireLocks, inspireRng);
    pushInspireHistory(std::move(idea));
    applyInspireIdea(inspireHistory[(size_t) inspireIndex], true);
}

void SynthesiaAIEditor::onInspireAgain()
{
    onInspireClicked();
}

void SynthesiaAIEditor::applyInspireIdea(const InspireIdea& idea, bool generate)
{
    if (!inspireLocks.tempo)
    {
        followHostTempoButton.setToggleState(false, juce::dontSendNotification);
        audioProcessor.setFollowHostTempo(false);
    }
    applyParamsToUI(idea.params);
    audioProcessor.loadGenreSounds(idea.params.genre);
    refreshInspireCaption();
    refreshMoodChips();
    resized();
    if (generate)
    {
        audioProcessor.setGeneratorType(GeneratorType::Genre);
        audioProcessor.generateMidi(idea.params);
        statusLabel.setText("Generating...", juce::dontSendNotification);
    }
}

void SynthesiaAIEditor::pushInspireHistory(InspireIdea idea)
{
    if (inspireIndex + 1 < (int) inspireHistory.size())
        inspireHistory.resize((size_t) inspireIndex + 1);
    inspireHistory.push_back(std::move(idea));
    while (inspireHistory.size() > 10)
        inspireHistory.erase(inspireHistory.begin());
    inspireIndex = (int) inspireHistory.size() - 1;
}

void SynthesiaAIEditor::showInspireHistory(int index)
{
    if (inspireHistory.empty())
        return;
    index = juce::jlimit(0, (int) inspireHistory.size() - 1, index);
    inspireIndex = index;
    applyInspireIdea(inspireHistory[(size_t) index], true);
}

void SynthesiaAIEditor::refreshInspireCaption()
{
    const bool has = inspireIndex >= 0 && inspireIndex < (int) inspireHistory.size();
    captionGenre.setVisible(has);
    captionKey.setVisible(has);
    captionTempo.setVisible(has);
    captionMood.setVisible(has);
    inspireBack.setVisible(has);
    inspireForward.setVisible(has);
    againButton.setVisible(has);
    if (!has)
        return;

    const auto& idea = inspireHistory[(size_t) inspireIndex];
    captionGenre.setText(idea.genreLabel);
    captionKey.setText(idea.keyLabel);
    captionTempo.setText(idea.tempoLabel);
    captionMood.setText(idea.moodLabel);
    captionGenre.setLocked(inspireLocks.genre);
    captionKey.setLocked(inspireLocks.key);
    captionTempo.setLocked(inspireLocks.tempo);
    captionMood.setLocked(inspireLocks.mood);
    inspireBack.setEnabled(inspireIndex > 0);
    inspireForward.setEnabled(inspireIndex + 1 < (int) inspireHistory.size());
}

void SynthesiaAIEditor::refreshMoodChips()
{
    InspireMood current = InspireMood::Chill;
    bool show = false;
    if (inspireLocks.mood)
    {
        for (auto* chip : moodChips)
        {
            if (chip->getToggleState())
            {
                current = chip->getMood();
                show = true;
            }
        }
        if (!show && inspireIndex >= 0 && inspireIndex < (int) inspireHistory.size())
        {
            current = inspireHistory[(size_t) inspireIndex].mood;
            show = true;
        }
    }
    for (auto* chip : moodChips)
        chip->setToggleState(show && chip->getMood() == current, juce::dontSendNotification);
}

void SynthesiaAIEditor::onRegeneratePart(Part part, bool variation)
{
    snapshotParams();
    statusLabel.setText(variation ? "Varying part..." : "Regenerating part...",
                        juce::dontSendNotification);
    audioProcessor.regeneratePart(part, variation);
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
    trackLanes.setClip(audioProcessor.getGeneratedMidi(),
                       audioProcessor.getClipLengthInBeats(),
                       audioProcessor.getProgression(),
                       audioProcessor.getSections());
    return true;
}

void SynthesiaAIEditor::onImportClicked()
{
    showImportPicker({});
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
            trackLanes.setClip(audioProcessor.getGeneratedMidi(),
                               audioProcessor.getClipLengthInBeats(),
                               audioProcessor.getProgression(),
                               audioProcessor.getSections());
        }
        else
        {
            statusLabel.setText("Capture was empty", juce::dontSendNotification);
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
    hideSettings();
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
        showImportPicker(mids.getFirst());
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
    auto* window = new juce::AlertWindow("Learn style",
                                         "Extract chords, voicings, rhythm and swing from MIDI.",
                                         juce::AlertWindow::NoIcon);
    window->addTextEditor("name", StyleProfile::sanitiseName(suggestedName), "Profile name");
    window->addButton("Learn", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, files, window](int result)
        {
            if (result != 1)
                return;
            if (!audioProcessor.learnStyle(files, window->getTextEditorContents("name")))
            {
                statusLabel.setText("Could not learn a style", juce::dontSendNotification);
                return;
            }
            shownStyleVersion = audioProcessor.getStyleVersion();
            refreshStyleCombo();
            planSummaryEditor.setText(audioProcessor.getCurrentStyle().summary);
            statusLabel.setText("Learned " + audioProcessor.getCurrentStyleName(),
                                juce::dontSendNotification);
        }), true);
}

void SynthesiaAIEditor::onLearnStyleClicked()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Learn style from MIDI...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.mid;*.midi");
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectDirectories
                             | juce::FileBrowserComponent::canSelectMultipleItems,
                         [this, chooser](const juce::FileChooser& fc)
                         {
                             auto results = fc.getResults();
                             if (results.isEmpty() && fc.getResult() != juce::File{})
                                 results.add(fc.getResult());
                             if (results.isEmpty())
                                 return;
                             learnFromFiles(results, results.getFirst().getFileName());
                         });
}

void SynthesiaAIEditor::onExportMidi()
{
    const auto present = MidiExporter::partsIn(audioProcessor.getGeneratedMidi());
    juce::PopupMenu menu;
    menu.addItem(1, "Export multi-track .mid");
    menu.addItem(2, "Export separate files per part", present.size() > 1);
    for (auto part : present)
        menu.addItem(10 + static_cast<int>(part), juce::String("Export ") + laneName(part));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),
                       [this](int result) { if (result != 0) exportToChooser(result); });
}

void SynthesiaAIEditor::exportToChooser(int menuId)
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save MIDI file...",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser, menuId](const juce::FileChooser& fc)
                         {
                             auto file = fc.getResult();
                             if (file == juce::File{})
                                 return;
                             if (!file.hasFileExtension(".mid"))
                                 file = file.withFileExtension(".mid");
                             const auto& midi = audioProcessor.getGeneratedMidi();
                             const double tempo = collectParams().tempo;
                             const int num = audioProcessor.getHostTimeSigNumerator();
                             const int den = audioProcessor.getHostTimeSigDenominator();
                             bool ok = false;
                             juce::String message;
                             if (menuId == 1)
                             {
                                 ok = MidiExporter::exportToFile(midi, file, tempo, num, den);
                                 message = "Exported " + file.getFileName();
                             }
                             else if (menuId == 2)
                             {
                                 ok = !MidiExporter::exportSeparateParts(midi, file, tempo).isEmpty();
                                 message = "Exported parts";
                             }
                             else
                             {
                                 const auto part = static_cast<Part>(menuId - 10);
                                 ok = MidiExporter::exportToFile(MidiExporter::filterPart(midi, part),
                                                                file, tempo, num, den);
                                 message = "Exported " + juce::String(laneName(part));
                             }
                             statusLabel.setText(ok ? message : "Export failed", juce::dontSendNotification);
                         });
}

void SynthesiaAIEditor::updateUIState()
{
    const bool busy = audioProcessor.isGenerating();
    const bool hasMidi = !audioProcessor.getGeneratedMidi().isEmpty();
    dragAllButton.setReady(hasMidi && !busy);
    generateButton.setEnabled(!busy);
    inspireButton.setEnabled(!busy);
    againButton.setEnabled(!busy);
    previewButton.setEnabled(hasMidi && !busy);
    previewButton.setKind(audioProcessor.isPreviewPlaying() ? IconKind::Stop : IconKind::Play);
    exportButton.setEnabled(hasMidi && !busy);
    trackLanes.setReady(hasMidi);
    pianoDrag.setReady(hasMidi);
    captureButton.setButtonText(audioProcessor.isCapturingMidi() ? "Stop capture" : "Capture MIDI");
    trackLanes.setMuteMask(audioProcessor.getMuteMask());
    pianoMute.setToggleState(audioProcessor.isPartMuted(Part::Piano), juce::dontSendNotification);
    pianoMute.setKind(audioProcessor.isPartMuted(Part::Piano) ? IconKind::Mute : IconKind::Unmute);

    if (audioProcessor.isAdvancedUi())
    {
        const bool licensed = audioProcessor.isLicensed();
        licenseBadge.setText(licensed ? "PRO" : "TRIAL", juce::dontSendNotification);
        licenseBadge.setColour(juce::Label::backgroundColourId, licensed ? Theme::licensed : Theme::trial);
        licenseBadge.setColour(juce::Label::textColourId, juce::Colours::black);
    }

    if (shownPresetVersion != audioProcessor.getPresetVersion())
    {
        shownPresetVersion = audioProcessor.getPresetVersion();
        refreshPresetCombo();
        applyParamsToUI(audioProcessor.getLastUiParams());
        shownUiParamsVersion = audioProcessor.getUiParamsVersion();
    }
    if (shownUiParamsVersion != audioProcessor.getUiParamsVersion() && !busy)
    {
        shownUiParamsVersion = audioProcessor.getUiParamsVersion();
        applyParamsToUI(audioProcessor.getLastUiParams());
    }
    if (shownStyleVersion != audioProcessor.getStyleVersion())
    {
        shownStyleVersion = audioProcessor.getStyleVersion();
        refreshStyleCombo();
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
        trackLanes.setClip(audioProcessor.getGeneratedMidi(),
                           audioProcessor.getClipLengthInBeats(),
                           audioProcessor.getProgression(),
                           audioProcessor.getSections());
    }
    trackLanes.setPlaybackBeat(audioProcessor.getPlaybackBeat());
    refreshPills();

    if (!busy && statusLabel.getText() == "Generating...")
    {
        juce::String ready = "Ready  ·  drag a part into your DAW";
        if (hasMidi && inspireIndex >= 0 && inspireIndex < (int) inspireHistory.size())
            ready = inspireHistory[(size_t) inspireIndex].caption;
        statusLabel.setText(hasMidi ? ready : "Generation failed", juce::dontSendNotification);
    }
    if (!busy && statusLabel.getText() == "Planning...")
    {
        const auto& plan = audioProcessor.getLastPlan();
        statusLabel.setText("Planned (" + juce::String(plan.source) + ")", juce::dontSendNotification);
        setMoreOpen(true);
    }
}

SettingsOverlay::SettingsOverlay(SynthesiaAIProcessor& p)
    : processor(p)
{
    title.setText("Settings", juce::dontSendNotification);
    title.setFont(Theme::medium(20.0f));
    title.setColour(juce::Label::textColourId, Theme::label);
    addAndMakeVisible(title);

    closeButton.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(closeButton);

    auto styleEditor = [](juce::TextEditor& editor)
    {
        editor.setColour(juce::TextEditor::backgroundColourId, Theme::clipBackground);
        editor.setColour(juce::TextEditor::outlineColourId, Theme::outline);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
        editor.setColour(juce::TextEditor::textColourId, Theme::label);
        editor.setFont(Theme::regular(15.0f));
    };

    setupLabel(providerLabel, "Provider");
    addAndMakeVisible(providerLabel);
    providerComboBox.addItem("OpenAI", 1);
    providerComboBox.addItem("Groq", 2);
    providerComboBox.addItem("Custom", 3);
    providerComboBox.onChange = [this]() { applyProviderDefaults(); };
    addAndMakeVisible(providerComboBox);

    setupLabel(apiKeyLabel, "API key");
    addAndMakeVisible(apiKeyLabel);
    apiKeyEditor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    apiKeyEditor.setTextToShowWhenEmpty("sk-... or gsk-...", Theme::dimText);
    styleEditor(apiKeyEditor);
    addAndMakeVisible(apiKeyEditor);

    setupLabel(endpointLabel, "Endpoint");
    addAndMakeVisible(endpointLabel);
    styleEditor(endpointEditor);
    addAndMakeVisible(endpointEditor);

    setupLabel(modelLabel, "Model");
    addAndMakeVisible(modelLabel);
    styleEditor(modelEditor);
    addAndMakeVisible(modelEditor);

    advancedToggle.setButtonText("Advanced UI");
    addAndMakeVisible(advancedToggle);

    setupLabel(licenseLabel, "License");
    addAndMakeVisible(licenseLabel);
    licenseEditor.setPasswordCharacter(static_cast<juce::juce_wchar>(0x2022));
    styleEditor(licenseEditor);
    addAndMakeVisible(licenseEditor);
    advancedToggle.onClick = [this]()
    {
        licenseLabel.setVisible(advancedToggle.getToggleState());
        licenseEditor.setVisible(advancedToggle.getToggleState());
        resized();
    };

    usageLabel.setFont(Theme::regular(13.0f));
    usageLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(usageLabel);

    hintLabel.setText("Keys stay on this machine. Without a key, Plan uses an offline heuristic.",
                      juce::dontSendNotification);
    hintLabel.setFont(Theme::regular(13.0f));
    hintLabel.setColour(juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible(hintLabel);

    saveButton.setButtonText("Save");
    saveButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    saveButton.onClick = [this]() { save(); };
    addAndMakeVisible(saveButton);

    cancelButton.setButtonText("Cancel");
    cancelButton.onClick = [this]() { if (onClose) onClose(); };
    addAndMakeVisible(cancelButton);

    refreshFromProcessor();
}

void SettingsOverlay::refreshFromProcessor()
{
    const auto provider = processor.getApiProvider();
    providerComboBox.setSelectedId(provider.equalsIgnoreCase("Groq") ? 2
                                   : provider.equalsIgnoreCase("Custom") ? 3 : 1,
                                   juce::dontSendNotification);
    apiKeyEditor.setText(processor.getApiKey(), juce::dontSendNotification);
    endpointEditor.setText(processor.getApiEndpoint(), juce::dontSendNotification);
    modelEditor.setText(processor.getApiModel(), juce::dontSendNotification);
    advancedToggle.setToggleState(processor.isAdvancedUi(), juce::dontSendNotification);
    licenseEditor.setText(processor.getLicenseKey(), juce::dontSendNotification);
    licenseLabel.setVisible(advancedToggle.getToggleState());
    licenseEditor.setVisible(advancedToggle.getToggleState());

    const auto last = processor.getLastTokenUsage();
    const auto session = processor.getSessionTokenUsage();
    juce::String usage;
    usage << "Last: " << last.promptTokens << " / " << last.completionTokens
          << "   Session: " << session.totalTokens << " tokens";
    usageLabel.setText(usage, juce::dontSendNotification);
    resized();
}

void SettingsOverlay::save()
{
    const auto id = providerComboBox.getSelectedId();
    processor.setApiProvider(id == 2 ? "Groq" : (id == 3 ? "Custom" : "OpenAI"));
    processor.setApiKey(apiKeyEditor.getText());
    processor.setApiEndpoint(endpointEditor.getText());
    processor.setApiModel(modelEditor.getText());
    processor.setAdvancedUi(advancedToggle.getToggleState());
    processor.setLicenseKey(licenseEditor.getText());
    if (onClose)
        onClose();
}

void SettingsOverlay::applyProviderDefaults()
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

juce::Rectangle<int> SettingsOverlay::cardBounds() const
{
    return getLocalBounds().withSizeKeepingCentre(440, advancedToggle.getToggleState() ? 560 : 500);
}

void SettingsOverlay::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black.withAlpha(0.52f));
    g.fillRect(getLocalBounds());
    Theme::fillGlass(g, cardBounds().toFloat(), 18.0f);
}

void SettingsOverlay::mouseUp(const juce::MouseEvent& e)
{
    if (!cardBounds().contains(e.getPosition()) && onClose)
        onClose();
}

void SettingsOverlay::resized()
{
    auto area = cardBounds().reduced(22);
    auto header = area.removeFromTop(28);
    closeButton.setBounds(header.removeFromRight(28));
    title.setBounds(header);
    area.removeFromTop(12);

    auto field = [&area](juce::Label& label, juce::Component& editor)
    {
        label.setBounds(area.removeFromTop(16));
        area.removeFromTop(4);
        editor.setBounds(area.removeFromTop(30));
        area.removeFromTop(10);
    };
    field(providerLabel, providerComboBox);
    field(apiKeyLabel, apiKeyEditor);
    field(endpointLabel, endpointEditor);
    field(modelLabel, modelEditor);
    advancedToggle.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    if (licenseEditor.isVisible())
        field(licenseLabel, licenseEditor);
    usageLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);
    hintLabel.setBounds(area.removeFromTop(36));
    auto buttons = area.removeFromBottom(36);
    saveButton.setBounds(buttons.removeFromRight(96));
    buttons.removeFromRight(10);
    cancelButton.setBounds(buttons.removeFromRight(96));
}
