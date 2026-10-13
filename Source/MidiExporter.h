#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include "Generators/MidiGenerator.h"

class MidiExporter
{
public:
    // Multi-track file: a tempo track plus one named track per part (MIDI channel) present.
    static bool exportToFile(const juce::MidiBuffer& midiBuffer, 
                           const juce::File& outputFile,
                           double tempo = 120.0,
                           int timeSignatureNumerator = 4,
                           int timeSignatureDenominator = 4);

    // Writes <name>-chords.mid, <name>-bass.mid, ... next to baseFile; returns the files written.
    static juce::Array<juce::File> exportSeparateParts(const juce::MidiBuffer& midiBuffer,
                                                       const juce::File& baseFile,
                                                       double tempo = 120.0);

    static juce::MidiBuffer filterPart(const juce::MidiBuffer& midiBuffer, Part part);
    static std::vector<Part> partsIn(const juce::MidiBuffer& midiBuffer);

private:
    MidiExporter() = delete;
};
