#pragma once

#include "MidiGenerator.h"
#include "../Music/MusicTheory.h"
#include <array>
#include <string>
#include <vector>

enum class ChordStyle { Sustained, Pulse, Stabs, Arpeggiated, Comping };
enum class BassStyle { Pattern, Walking };
enum class LeadStyle { Melody, Arp };
enum class PianoStyle { Broken, Comp, Melodic, Arpeggio };

struct ProgressionTemplate
{
    std::string tokens;     // see theory::parseChordToken
    double barsPerChord{1.0};
};

struct MelodyProfile
{
    int low{60};
    int high{84};
    double density{0.5};        // 0..1, chance of onsets on metrically strong steps
    double syncopation{0.3};    // extra weight on off-beats
    double restChance{0.3};     // breathing room / detached notes
    double longNoteChance{0.2};
    int motifBars{2};
    bool useExtensions{false};  // 7ths/9ths count as chord tones on strong beats
    bool dyads{false};          // add a harmony note under strong beats
    int velocity{90};
    std::array<double, 13> intervalWeights{}; // signed intervals -6..+6; all-zero = unused
};

struct ArpProfile
{
    int low{60};
    int high{88};
    std::vector<std::vector<int>> shapes;   // index sequences into the stacked chord tones
    double gate{0.8};
    int velocity{88};
};

// Drum pattern strings are 16 steps (one bar of 16ths): 'X' accent, 'x' hit, 'g' ghost, '.' rest.
struct DrumProfile
{
    std::vector<std::string> kick;
    std::vector<std::string> snare;
    std::vector<std::string> clap;
    std::vector<std::string> closedHat;
    std::vector<std::string> openHat;
    std::vector<std::string> perc;          // rim, shaker, tambourine
    bool rideHats{false};                   // lo-fi/jazz: ride instead of busy closed hats
    double hatSwing{0.5};                   // 0.5 = straight; 16th shuffle when > 0.5
    double laidBackMs{0.0};                 // delay the whole kit (Dilla / laid-back)
    double hatLagMs{0.0};                   // extra delay on hats only
    double humanizeMs{5.0};
    double ghostSnareChance{0.15};
    double fillChance{0.8};
    double hatRollChance{0.0};
    bool lockKickToBass{false};
    int kickVelocity{110};
    int snareVelocity{108};
    int hatVelocity{88};
};

// Pattern strings are 16 steps (one bar of 16ths):
//   chords/piano: 'X' accent, 'x' hit, 'g' ghost, '-' hold, '.' rest
//   bass:         'R' root, 'F' fifth, 'O' octave, 'T' third, 'A' approach to next root,
//                 lowercase = ghost note, '-' hold, '.' rest
struct GenreProfile
{
    Genre genre;
    double defaultTempo;
    Quantization defaultGrid;
    int defaultExtension;   // 3 triads, 7, 9, 11

    std::vector<ProgressionTemplate> majorProgressions;
    std::vector<ProgressionTemplate> minorProgressions;

    ChordStyle chordStyle;
    std::vector<std::string> chordPatterns;
    theory::VoicingOptions chordVoicing;
    ArpProfile chordArp;

    BassStyle bassStyle;
    std::vector<std::string> bassPatterns;
    int bassLow;
    int bassHigh;
    double slideChance;

    LeadStyle leadStyle;
    MelodyProfile leadMelody;
    ArpProfile leadArp;

    PianoStyle pianoStyle;
    std::vector<std::string> pianoPatterns;
    theory::VoicingOptions pianoVoicing;
    MelodyProfile pianoMelody;
    ArpProfile pianoArp;

    double swing;           // 0.5 = straight, e.g. 0.62 = swung
    bool swingSixteenths;
    double anticipation;    // chance a hit just before a chord change plays the next chord

    DrumProfile drums;
};

const GenreProfile& getGenreProfile(Genre genre);
