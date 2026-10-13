#pragma once

#include "Generators/MidiGenerator.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <string>
#include <vector>

struct MidiAnalysis
{
    bool ok{false};
    juce::String error;
    juce::String sourceName;
    double tempo{120.0};
    int rootNote{0};
    ScaleType scale{ScaleType::Major};
    int lengthInBars{8};
    double lengthInBeats{32.0};
    std::vector<ChordInfo> progression;
    std::string chordChart;
    Part primaryPart{Part::Piano};
    int presentParts{0};
    juce::MidiBuffer remapped;
    std::string motifContour{ "arch" };
    std::string motifRhythm{ "straight" };
    std::string summary;
};

class MidiImporter
{
public:
    static MidiAnalysis analyseFile(const juce::File& file);
    static MidiAnalysis analyseBuffer(const juce::MidiBuffer& midi, double ticksPerQuarter, double tempo,
                                      const juce::String& sourceName = "clip");
    static juce::MidiBuffer overlay(const juce::MidiBuffer& generated, const MidiAnalysis& imported,
                                    ImportMode mode, int extraKeepMask, double generatedBeats);
    static bool writePianoMelody(const juce::File& file);
    static bool writeChordLoop(const juce::File& file);

private:
    MidiImporter() = delete;
};
