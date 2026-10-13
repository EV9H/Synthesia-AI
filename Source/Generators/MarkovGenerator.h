#pragma once

#include "MidiGenerator.h"
#include <random>
#include <map>

class MarkovGenerator : public MidiGenerator
{
public:
    MarkovGenerator();
    ~MarkovGenerator() override = default;
    
    juce::MidiBuffer generate(const MidiGenerationParams& params) override;

private:
    std::mt19937 rng;
    
    std::vector<Note> generateMarkovSequence(const MidiGenerationParams& params);
    int selectNextNote(int currentNote, const std::vector<int>& scale);
    double generateNoteDuration(Quantization quant);
};
