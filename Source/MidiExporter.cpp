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
    
    // getMessage() already carries samplePosition (ticks) as its timestamp;
    // addEvent's second argument is an extra offset on top of that.
    for (const auto metadata : midiBuffer)
    {
        track.addEvent(metadata.getMessage(), 0.0);
    }
    
    track.updateMatchedPairs();
    
    // Add track to MIDI file
    midiFile.addTrack(track);
    
    // Set ticks per quarter note (standard MIDI resolution)
    midiFile.setTicksPerQuarterNote(960);
    
    juce::FileOutputStream stream(outputFile);
    
    if (!stream.openedOk())
        return false;

    // FileOutputStream appends to existing files; overwrite instead.
    stream.setPosition(0);
    stream.truncate();
    
    return midiFile.writeTo(stream);
}
