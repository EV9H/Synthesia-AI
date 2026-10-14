#pragma once

#include "../Generators/MidiGenerator.h"
#include <array>
#include <random>
#include <string>
#include <vector>

namespace theory
{
    // Seven-note mode used for harmony. Pentatonic harmonises with its parent major scale.
    std::array<int, 7> modeIntervals(ScaleType scale);

    // Pitch classes (relative to the key root) available to melodies.
    std::vector<int> melodyIntervals(ScaleType scale);

    bool isMinorMode(ScaleType scale);

    std::string pitchClassName(int pitchClass, bool preferFlats);
    bool keyPrefersFlats(int keyRoot, ScaleType scale);

    // Chord tokens, one per chord slot:
    //   [b]<degree 1-7>[D][ext]
    //   b    borrowed chord, a major chord on the flattened degree (bVI, bVII, bIII, bII)
    //   D    dominant quality (major third, minor seventh), e.g. "5D" for V7 in minor
    //   ext  "t" triad, "7", "9", "11", "s2"/"s4" suspended, "a9" add9, "h" half-dim
    // Without ext the genre's default extension level is used.
    struct ChordSpec
    {
        int degree{0};          // 0-based scale degree
        bool borrowed{false};
        bool dominant{false};
        int extension{0};       // 0 = genre default, else 3/7/9/11
        int suspension{0};      // 0, 2 or 4
        bool add9{false};
        bool halfDiminished{false};
    };

    ChordSpec parseChordToken(const std::string& token);

    // Builds root + intervals (ascending, relative to root, may exceed an octave).
    ChordInfo buildChord(int keyRoot, ScaleType scale, const ChordSpec& spec, int defaultExtension);

    // User-facing symbols: "Am9", "Fmaj9", "E7b9", "vi", "V7", "bVI".
    bool parseUserChord(const std::string& token, int keyRoot, ScaleType scale,
                        int defaultExtension, ChordInfo& out);

    // Space-separated chords, optionally grouped per bar with '|'. Repeats to fill `bars`.
    std::vector<ChordInfo> parseProgressionText(const std::string& text, int keyRoot,
                                                ScaleType scale, int bars, int defaultExtension);

    std::string chordSymbol(int rootPitchClass, const std::vector<int>& intervals, bool preferFlats);

    std::vector<int> chordPitchClasses(const ChordInfo& chord);
    bool containsPitchClass(const ChordInfo& chord, int pitch);

    // Nearest MIDI pitch with the given pitch class to `target`.
    int nearestPitchWithClass(int pitchClass, int target);

    struct VoicingOptions
    {
        int low{48};
        int high{76};
        int maxNotes{5};
        bool rootless{false};   // drop the root (bass covers it) for 4+ note chords
        bool open{false};       // spread voicings (cinematic / pads)
    };

    // Picks the inversion/voicing that moves least from `previous` (voice leading).
    std::vector<int> voiceChord(const ChordInfo& chord, const std::vector<int>& previous,
                                const VoicingOptions& options, std::mt19937& rng);
}
