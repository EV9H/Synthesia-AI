#pragma once

#include "Generators/GenreProfiles.h"
#include "MidiImporter.h"
#include <array>
#include <map>
#include <random>
#include <string>
#include <vector>

struct PartStyleStats
{
    std::array<double, 16> onsetGrid{};
    double density{0.0};          // notes per beat
    double velocityMean{80.0};
    int registerLow{127};
    int registerHigh{0};
};

// Statistics extracted from one or more user MIDI files. Blended with a GenreProfile
// at generate time so the arranger (and LLM planner) follow the user's tendencies.
struct StyleProfile
{
    std::string name;
    std::vector<std::string> sources;
    int filesLearned{0};

    double tempo{90.0};
    double swing{0.5};
    bool swingSixteenths{false};
    double laidBackMs{0.0};
    int defaultExtension{7};
    int rootNote{0};
    ScaleType scale{ScaleType::Minor};

    std::vector<std::string> chordVocabulary; // degree tokens, e.g. "19", "411", "5D9"
    std::map<std::string, std::map<std::string, double>> chordTransitions;
    double voicingSpread{14.0};
    int voicingMaxNotes{4};
    bool rootless{false};
    int voicingLow{48};
    int voicingHigh{76};

    PartStyleStats chords, bass, synth, piano, drums;
    std::array<double, 13> melodyIntervals{};
    std::string motifContour{ "arch" };
    std::string motifRhythm{ "straight" };
    double chromaticApproach{0.0};
    std::vector<std::string> bassPatterns;
    std::vector<std::string> chordPatterns;
    std::vector<std::string> pianoPatterns;
    std::string summary;

    bool empty() const { return filesLearned <= 0 && chordVocabulary.empty(); }

    juce::String toJson() const;
    static StyleProfile fromJson(const juce::String& json);

    static StyleProfile extract(const MidiAnalysis& analysis);
    static StyleProfile learnFromFiles(const juce::Array<juce::File>& files);
    static StyleProfile learnFromFolder(const juce::File& folder);
    void absorb(const StyleProfile& other);
    void finaliseSummary();

    static GenreProfile blend(const GenreProfile& genre, const StyleProfile& style, double amount);
    static GenreProfile effectiveGenre(const MidiGenerationParams& params);
    static std::vector<ChordInfo> sampleProgression(const StyleProfile& style,
                                                    const MidiGenerationParams& params,
                                                    std::mt19937& rng);
    juce::String promptHint() const;

    static juce::File storeDirectory();
    static juce::StringArray listSaved();
    static bool saveNamed(const StyleProfile& profile);
    static StyleProfile loadNamed(const juce::String& name);
    static juce::String sanitiseName(const juce::String& name);

    static bool writeNeoSoulFixtures(const juce::File& dir);
};
