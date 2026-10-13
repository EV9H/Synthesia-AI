#include "MidiGenerator.h"

std::vector<int> MidiGenerator::getScaleNotes(int rootNote, ScaleType scale)
{
    std::vector<int> intervals;
    
    switch (scale)
    {
        case ScaleType::Major:
            intervals = {0, 2, 4, 5, 7, 9, 11};
            break;
        case ScaleType::Minor:
            intervals = {0, 2, 3, 5, 7, 8, 10};
            break;
        case ScaleType::Dorian:
            intervals = {0, 2, 3, 5, 7, 9, 10};
            break;
        case ScaleType::Mixolydian:
            intervals = {0, 2, 4, 5, 7, 9, 10};
            break;
        case ScaleType::Pentatonic:
            intervals = {0, 2, 4, 7, 9};
            break;
    }
    
    std::vector<int> scaleNotes;
    for (int octave = 3; octave <= 6; ++octave)
    {
        for (int interval : intervals)
        {
            int note = octave * 12 + rootNote + interval;
            if (note >= 0 && note < 128)
                scaleNotes.push_back(note);
        }
    }
    
    return scaleNotes;
}

juce::MidiBuffer MidiGenerator::notesToMidiBuffer(const std::vector<Note>& notes,
                                                   double tempo,
                                                   double ppq)
{
    juce::MidiBuffer buffer;
    
    // Convert quarter notes to ticks (ppq = pulses per quarter note)
    for (const auto& note : notes)
    {
        int startTick = static_cast<int>(note.startTime * ppq);
        int duration = static_cast<int>(note.duration * ppq);
        
        buffer.addEvent(juce::MidiMessage::noteOn(1, note.pitch, 
                       static_cast<juce::uint8>(note.velocity)), startTick);
        buffer.addEvent(juce::MidiMessage::noteOff(1, note.pitch), 
                       startTick + duration);
    }
    
    return buffer;
}

double MidiGenerator::quantizationToQuarterNotes(Quantization quant)
{
    switch (quant)
    {
        case Quantization::Sixteenth: return 0.25;
        case Quantization::Eighth:    return 0.5;
        case Quantization::Quarter:   return 1.0;
        case Quantization::Half:      return 2.0;
        default: return 0.5;
    }
}
