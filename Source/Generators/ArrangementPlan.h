#pragma once

#include "MidiGenerator.h"
#include <juce_core/juce_core.h>
#include <array>
#include <string>
#include <vector>

// Compact arrangement plan. The LLM (or a keyword heuristic) fills this in;
// the rule-based arranger realises every note so nothing is ever out of key.
//
// JSON schema (all fields optional; missing values are repaired):
// {
//   "genre": "pop|lofi|house|trap|rnb|cinematic",
//   "key": "C"|"C#"|"Db"|...|"B",
//   "scale": "major|minor|dorian|mixolydian|pentatonic",
//   "tempo": 60-180,
//   "swing": 0.5-0.75,
//   "bars": 8-32,
//   "parts": { "chords": true, "bass": true, "synth": true, "piano": true, "drums": true },
//   "motif": { "contour": "rising|falling|arch|static", "rhythm": "straight|swung|syncopated|sparse" },
//   "sections": [
//     { "name": "intro|verse|pre|chorus|bridge|drop|break|outro",
//       "bars": 1-16,
//       "energy": 0-1,
//       "chords": "Am9 Fmaj9",
//       "density": { "chords": 0-1, "bass": 0-1, "synth": 0-1, "piano": 0-1, "drums": 0-1 } }
//   ]
// }

struct TokenUsage
{
    int promptTokens{0};
    int completionTokens{0};
    int totalTokens{0};
    int requests{0};
};

struct MotifHint
{
    std::string contour{ "arch" };
    std::string rhythm{ "straight" };
};

struct PlannedSection
{
    SectionKind kind{SectionKind::Verse};
    int bars{4};
    float energy{0.7f};
    std::string chords;
    float densityChords{0.7f};
    float densityBass{0.7f};
    float densitySynth{0.7f};
    float densityPiano{0.7f};
    float densityDrums{0.7f};
};

struct ArrangementPlan
{
    Genre genre{Genre::Pop};
    int rootNote{0};
    ScaleType scale{ScaleType::Major};
    double tempo{120.0};
    double swing{0.5};
    int lengthInBars{24};
    MotifHint motif;
    std::array<bool, 5> parts{ true, true, true, true, true }; // chords, bass, synth, piano, drums
    std::vector<PlannedSection> sections;
    TokenUsage usage;
    std::string source{ "heuristic" };  // "llm" | "heuristic" | "json"
    std::string prompt;

    static const char* schemaPrompt();

    static ArrangementPlan parseJson(const juce::String& json, const MidiGenerationParams& fallback);
    static ArrangementPlan fromKeywords(const std::string& prompt, const MidiGenerationParams& fallback);
    static void validateAndRepair(ArrangementPlan& plan, const MidiGenerationParams& fallback);

    juce::String toJson() const;
    std::string chordChart() const;
    std::string summary() const;
    std::vector<SectionInfo> toSectionInfo() const;
    std::vector<ChordInfo> toProgression() const;
    void applyTo(MidiGenerationParams& params) const;
    bool partEnabled(Part part) const;
    int enabledMask() const;
};

Genre genreFromSlug(const juce::String& text);
int keyFromName(const juce::String& text);
ScaleType scaleFromName(const juce::String& text);
SectionKind sectionFromName(const juce::String& text);

inline const char* genreSlug(Genre genre)
{
    switch (genre)
    {
        case Genre::Pop:       return "pop";
        case Genre::LoFiJazz:  return "lofi";
        case Genre::House:     return "house";
        case Genre::Trap:      return "trap";
        case Genre::RnB:       return "rnb";
        case Genre::Cinematic: return "cinematic";
    }
    return "pop";
}
