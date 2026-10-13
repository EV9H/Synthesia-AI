#include "MidiExporter.h"

bool MidiExporter::exportToFile(const juce::MidiBuffer& midiBuffer,
                               const juce::File& outputFile,
                               double tempo,
                               int timeSignatureNumerator,
                               int timeSignatureDenominator)
{
    juce::MidiFile midiFile;
    
    // Create a track
    juce::MidiMessageSequence track;
    
    // Add tempo meta event at the start
    track.addEvent(juce::MidiMessage::tempoMetaEvent(
        static_cast<int>(60000000.0 / tempo)), 0.0);
    
    // Add time signature
    track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(
        timeSignatureNumerator, timeSignatureDenominator), 0.0);
    
    // Add all MIDI events from buffer
    for (const auto metadata : midiBuffer)
    {
        track.addEvent(metadata.getMessage(), metadata.samplePosition);
    }
    
    track.updateMatchedPairs();
    
    // Add track to MIDI file
    midiFile.addTrack(track);
    
    // Set ticks per quarter note (standard MIDI resolution)
    midiFile.setTicksPerQuarterNote(960);
    
    // Write to file
    juce::FileOutputStream stream(outputFile);
    
    if (!stream.openedOk())
        return false;
    
    return midiFile.writeTo(stream);
}
