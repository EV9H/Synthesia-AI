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

// Values are persisted in plugin state; append new types at the end.
enum class GeneratorType
{
    Markov,
    LLM,
    Genre
};

enum class Genre
{
    Pop = 0,
    LoFiJazz,
    House,
    Trap,
    RnB,
    Cinematic
};

constexpr int numGenres = 6;

inline const char* genreName(Genre genre)
{
    switch (genre)
    {
        case Genre::Pop:       return "Pop";
        case Genre::LoFiJazz:  return "Lo-fi / Jazz";
        case Genre::House:     return "EDM / House";
        case Genre::Trap:      return "Trap / Hip-hop";
        case Genre::RnB:       return "R&B / Neo-soul";
        case Genre::Cinematic: return "Cinematic";
    }
    return "Pop";
}

// Each part is generated on its own MIDI channel so it can be split into tracks.
enum class Part
{
    All = 0,
    Chords,
    Bass,
    Synth,
    Piano,
    Drums
};

constexpr Part allParts[] = { Part::Chords, Part::Bass, Part::Synth, Part::Piano, Part::Drums };

inline const char* partName(Part part)
{
    switch (part)
    {
        case Part::All:    return "All parts";
        case Part::Chords: return "Chords";
        case Part::Bass:   return "Bass";
        case Part::Synth:  return "Synth";
        case Part::Piano:  return "Piano";
        case Part::Drums:  return "Drums";
    }
    return "All parts";
}

// Melodic parts sit on channels 1-4; drums use GM channel 10 so exported
// files play a drum kit in any General MIDI rack.
inline int partChannel(Part part)
{
    if (part == Part::All)   return 0;
    if (part == Part::Drums) return 10;
    return static_cast<int>(part);
}

inline Part partFromChannel(int channel)
{
    if (channel == 10) return Part::Drums;
    return (channel >= 1 && channel <= 4) ? static_cast<Part>(channel) : Part::Synth;
}

// A chord placed on the timeline. Intervals are semitones above the root.
struct ChordInfo
{
    double startBeat{0.0};
    double lengthBeats{4.0};
    int rootPitchClass{0};
    std::vector<int> intervals;
    std::string symbol;
};

enum class ArrangementForm
{
    Loop = 0,
    Song
};

enum class ImportMode
{
    None = 0,
    Accompany,
    Extend,
    Rewrite,
    Restyle
};

inline const char* importModeName(ImportMode mode)
{
    switch (mode)
    {
        case ImportMode::Accompany: return "Accompany";
        case ImportMode::Extend:    return "Extend";
        case ImportMode::Rewrite:   return "Rewrite";
        case ImportMode::Restyle:   return "Restyle";
        case ImportMode::None:      return "Off";
    }
    return "Off";
}

enum class SectionKind
{
    Intro,
    Verse,
    PreChorus,
    Chorus,
    Bridge,
    Drop,
    Break,
    Outro
};

struct SectionInfo
{
    SectionKind kind{SectionKind::Verse};
    double startBeat{0.0};
    double lengthBeats{4.0};
    float energy{0.7f};
    int occurrence{1};
    std::string label;
    float densityChords{0.7f};
    float densityBass{0.7f};
    float densitySynth{0.7f};
    float densityPiano{0.7f};
    float densityDrums{0.7f};
};

inline const char* sectionName(SectionKind kind)
{
    switch (kind)
    {
        case SectionKind::Intro:     return "Intro";
        case SectionKind::Verse:     return "Verse";
        case SectionKind::PreChorus: return "Pre";
        case SectionKind::Chorus:    return "Chorus";
        case SectionKind::Bridge:    return "Bridge";
        case SectionKind::Drop:      return "Drop";
        case SectionKind::Break:     return "Break";
        case SectionKind::Outro:     return "Outro";
    }
    return "Verse";
}

inline const char* formName(ArrangementForm form)
{
    return form == ArrangementForm::Song ? "Song form" : "Loop";
}

inline int partLockBit(Part part)
{
    return 1 << static_cast<int>(part);
}

inline bool isPartLocked(int mask, Part part)
{
    return (mask & partLockBit(part)) != 0;
}

struct MidiGenerationParams
{
    int rootNote{0};  // 0 = C, 1 = C#, etc.
    ScaleType scale{ScaleType::Major};
    int lengthInBars{4};
    Quantization quantization{Quantization::Eighth};
    double tempo{120.0};
    std::string stylePrompt;
    unsigned int seed{0};  // 0 = non-deterministic
    Genre genre{Genre::Pop};
    Part part{Part::All};
    // Harmonic context to write over; empty = the generator chooses a progression
    std::vector<ChordInfo> progression;
    ArrangementForm form{ArrangementForm::Loop};
    std::string chordText;
    bool lockProgression{false};
    int lockedParts{0};
    // 0 = use the genre profile's swing; otherwise 0.5 (straight) to ~0.75
    double swing{0.0};
    std::string motifContour;   // rising | falling | arch | static
    std::string motifRhythm;    // straight | swung | syncopated | sparse
    // 0 = every part on; otherwise a mask of partLockBit()
    int enabledParts{0};
    std::vector<SectionInfo> plannedSections;
    std::string planJson;
    ImportMode importMode{ImportMode::None};
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
                                       double ppq = 960.0,
                                       int channel = 1);
    double quantizationToQuarterNotes(Quantization quant);
};
