#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

class MidiExporter
{
public:
    static bool exportToFile(const juce::MidiBuffer& midiBuffer, 
                           const juce::File& outputFile,
                           double tempo = 120.0,
                           int timeSignatureNumerator = 4,
                           int timeSignatureDenominator = 4);

private:
    MidiExporter() = delete;
};
