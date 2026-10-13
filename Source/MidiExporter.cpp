#include "MidiExporter.h"

namespace
{
    // General MIDI programs (0-based) so DAWs pick sensible default instruments
    int gmProgramFor(Part part)
    {
        switch (part)
        {
            case Part::Chords: return 89;   // Pad 2 (warm)
            case Part::Bass:   return 38;   // Synth Bass 1
            case Part::Synth:  return 81;   // Lead 2 (sawtooth)
            case Part::Piano:  return 0;    // Acoustic Grand Piano
            case Part::Drums:  return 0;    // Standard GM kit (channel 10)
            case Part::All:    break;
        }
        return 0;
    }

    bool writeFile(const juce::MidiFile& midiFile, const juce::File& outputFile)
    {
        outputFile.getParentDirectory().createDirectory();
        juce::FileOutputStream stream(outputFile);

        if (!stream.openedOk())
            return false;

        // FileOutputStream appends to existing files; overwrite instead.
        stream.setPosition(0);
        stream.truncate();

        return midiFile.writeTo(stream);
    }
}

juce::MidiBuffer MidiExporter::filterPart(const juce::MidiBuffer& midiBuffer, Part part)
{
    juce::MidiBuffer filtered;
    for (const auto metadata : midiBuffer)
    {
        const auto message = metadata.getMessage();
        if (part == Part::All || message.getChannel() == partChannel(part))
            filtered.addEvent(message, metadata.samplePosition);
    }
    return filtered;
}

std::vector<Part> MidiExporter::partsIn(const juce::MidiBuffer& midiBuffer)
{
    std::vector<Part> parts;
    for (const auto part : allParts)
    {
        for (const auto metadata : midiBuffer)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn() && message.getChannel() == partChannel(part))
            {
                parts.push_back(part);
                break;
            }
        }
    }
    return parts;
}

bool MidiExporter::exportToFile(const juce::MidiBuffer& midiBuffer,
                               const juce::File& outputFile,
                               double tempo,
                               int timeSignatureNumerator,
                               int timeSignatureDenominator)
{
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);

    juce::MidiMessageSequence conductor;
    conductor.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(60000000.0 / tempo)), 0.0);
    conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(
        timeSignatureNumerator, timeSignatureDenominator), 0.0);

    for (const auto metadata : midiBuffer)
    {
        const auto message = metadata.getMessage();
        if (message.isMetaEvent() && message.getMetaEventType() == 6)
            conductor.addEvent(message, 0.0);
    }

    midiFile.addTrack(conductor);

    for (const auto part : partsIn(midiBuffer))
    {
        juce::MidiMessageSequence track;
        track.addEvent(juce::MidiMessage::textMetaEvent(3, partName(part)), 0.0);
        track.addEvent(juce::MidiMessage::programChange(partChannel(part), gmProgramFor(part)), 0.0);

        // getMessage() already carries samplePosition (ticks) as its timestamp;
        // addEvent's second argument is an extra offset on top of that.
        for (const auto metadata : filterPart(midiBuffer, part))
            track.addEvent(metadata.getMessage(), 0.0);

        track.updateMatchedPairs();
        midiFile.addTrack(track);
    }

    return writeFile(midiFile, outputFile);
}

juce::Array<juce::File> MidiExporter::exportSeparateParts(const juce::MidiBuffer& midiBuffer,
                                                          const juce::File& baseFile,
                                                          double tempo)
{
    juce::Array<juce::File> written;

    for (const auto part : partsIn(midiBuffer))
    {
        const auto file = baseFile.getSiblingFile(baseFile.getFileNameWithoutExtension() + "-"
                                                  + juce::String(partName(part)).toLowerCase() + ".mid");
        if (exportToFile(filterPart(midiBuffer, part), file, tempo))
            written.add(file);
    }

    return written;
}
