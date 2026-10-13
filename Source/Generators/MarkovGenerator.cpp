#include "MarkovGenerator.h"

MarkovGenerator::MarkovGenerator()
    : rng(std::random_device{}())
{
}

juce::MidiBuffer MarkovGenerator::generate(const MidiGenerationParams& params)
{
    auto notes = generateMarkovSequence(params);
    return notesToMidiBuffer(notes, params.tempo);
}

std::vector<Note> MarkovGenerator::generateMarkovSequence(const MidiGenerationParams& params)
{
    std::vector<Note> notes;
    auto scaleNotes = getScaleNotes(params.rootNote, params.scale);
    
    if (scaleNotes.empty())
        return notes;
    
    double totalQuarterNotes = params.lengthInBars * 4.0; // 4 quarter notes per bar
    double currentTime = 0.0;
    
    // Start with root note in middle octave
    int currentNote = scaleNotes[scaleNotes.size() / 2];
    
    while (currentTime < totalQuarterNotes)
    {
        double duration = generateNoteDuration(params.quantization);
        
        // Don't exceed total length
        if (currentTime + duration > totalQuarterNotes)
            duration = totalQuarterNotes - currentTime;
        
        Note note;
        note.pitch = currentNote;
        note.startTime = currentTime;
        note.duration = duration;
        note.velocity = 80 + (rng() % 30); // Velocity variation 80-110
        
        notes.push_back(note);
        
        currentTime += duration;
        currentNote = selectNextNote(currentNote, scaleNotes);
    }
    
    return notes;
}

int MarkovGenerator::selectNextNote(int currentNote, const std::vector<int>& scale)
{
    // Simple Markov: prefer stepwise motion (±1 or ±2 scale degrees)
    // with occasional jumps
    
    auto it = std::find(scale.begin(), scale.end(), currentNote);
    if (it == scale.end())
        return scale[scale.size() / 2];
    
    int currentIndex = static_cast<int>(std::distance(scale.begin(), it));
    
    std::uniform_real_distribution<> dist(0.0, 1.0);
    double rand = dist(rng);
    
    int newIndex;
    if (rand < 0.4) // 40% step up or down
    {
        newIndex = currentIndex + (rng() % 2 == 0 ? 1 : -1);
    }
    else if (rand < 0.7) // 30% small jump
    {
        newIndex = currentIndex + (rng() % 2 == 0 ? 2 : -2);
    }
    else // 30% larger jump
    {
        newIndex = currentIndex + (rng() % 7 - 3);
    }
    
    // Clamp to scale range
    newIndex = juce::jlimit(0, static_cast<int>(scale.size()) - 1, newIndex);
    
    return scale[newIndex];
}

double MarkovGenerator::generateNoteDuration(Quantization quant)
{
    double baseLength = quantizationToQuarterNotes(quant);
    
    // Occasionally double or halve the duration
    std::uniform_real_distribution<> dist(0.0, 1.0);
    double rand = dist(rng);
    
    if (rand < 0.1)
        return baseLength * 2.0;
    else if (rand < 0.2)
        return baseLength * 0.5;
    else
        return baseLength;
}
