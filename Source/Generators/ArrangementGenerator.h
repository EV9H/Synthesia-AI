#pragma once

#include "MidiGenerator.h"
#include "GenreProfiles.h"
#include <map>
#include <random>

struct GenerationResult
{
    juce::MidiBuffer midi;                  // ticks at 960 PPQ, one MIDI channel per part
    std::vector<ChordInfo> progression;
};

// Genre-aware arranger: builds a chord progression, then chords, bass, synth, piano
// and drums parts that follow it. Every part is always generated (each from its own
// random stream) and then filtered, so a solo part matches the same part inside the mix.
class ArrangementGenerator : public MidiGenerator
{
public:
    juce::MidiBuffer generate(const MidiGenerationParams& params) override;

    GenerationResult generateArrangement(const MidiGenerationParams& params);

    static std::vector<ChordInfo> buildProgression(const MidiGenerationParams& params);

    struct PartNote
    {
        int pitch;
        double start;
        double length;
        int velocity;
        bool glideToNext{false};
        int pitchBend{8192};    // 14-bit, 8192 = centre; used for hat pitch ramps
    };
    using PartNotes = std::vector<PartNote>;

private:
    struct Context
    {
        const MidiGenerationParams& params;
        const GenreProfile& profile;
        const std::vector<ChordInfo>& progression;
        double totalBeats;
        int finestSteps;    // smallest melodic subdivision in 16th steps
    };

    static std::vector<ChordInfo> buildProgression(const MidiGenerationParams& params, std::mt19937& rng);

    PartNotes generateChords(const Context& ctx, std::mt19937& rng);
    PartNotes generateBass(const Context& ctx, std::mt19937& rng);
    PartNotes generateSynth(const Context& ctx, std::mt19937& rng);
    PartNotes generatePiano(const Context& ctx, std::mt19937& rng);
    PartNotes generateDrums(const Context& ctx, const PartNotes& bassNotes, std::mt19937& rng);

    PartNotes generatePatternBass(const Context& ctx, std::mt19937& rng);
    PartNotes generateWalkingBass(const Context& ctx, std::mt19937& rng);
    PartNotes generateMelody(const Context& ctx, const MelodyProfile& role, std::mt19937& rng);
    PartNotes generateArp(const Context& ctx, const ArpProfile& role, int rateSteps, std::mt19937& rng);
    PartNotes generatePatternChords(const Context& ctx, const std::vector<std::string>& patterns,
                                    const theory::VoicingOptions& voicing, int baseVelocity,
                                    std::mt19937& rng);
    PartNotes generateBrokenPiano(const Context& ctx, std::mt19937& rng);

    static void addSlides(PartNotes& notes, double chance, std::mt19937& rng);
    static void applySwing(PartNotes& notes, double swing, bool sixteenths);
    static void finalise(PartNotes& notes, double totalBeats);
    static void appendToBuffer(const PartNotes& notes, int channel, int bendRangeSemitones,
                               juce::MidiBuffer& buffer);
};
