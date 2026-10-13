#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <string>
#include <vector>
#include <algorithm>

enum class ScaleType
{
    Major = 0,
    Minor,
    Dorian,
    Mixolydian,
    Pentatonic
};

enum class Quantization
{
    Sixteenth = 0,
    Eighth,
    Quarter,
    Half
};

enum class GeneratorType
{
    Markov,
    LLM
};

struct MidiGenerationParams
{
    int rootNote{0};  // 0 = C, 1 = C#, etc.
    ScaleType scale{ScaleType::Major};
    int lengthInBars{4};
    Quantization quantization{Quantization::Eighth};
    double tempo{120.0};
    std::string stylePrompt;
    unsigned int seed{0};  // 0 = non-deterministic
};

struct Note
{
    int pitch;
    double startTime;  // in quarter notes
    double duration;   // in quarter notes
    int velocity;
};

class MidiGenerator
{
public:
    virtual ~MidiGenerator() = default;
    
    virtual juce::MidiBuffer generate(const MidiGenerationParams& params) = 0;
    
protected:
    std::vector<int> getScaleNotes(int rootNote, ScaleType scale);
    juce::MidiBuffer notesToMidiBuffer(const std::vector<Note>& notes, 
                                       double tempo, 
                                       double ppq = 960.0);
    double quantizationToQuarterNotes(Quantization quant);
};
