#include "MusicTheory.h"
#include <cmath>
#include <set>

namespace theory
{
    std::array<int, 7> modeIntervals(ScaleType scale)
    {
        switch (scale)
        {
            case ScaleType::Minor:      return { 0, 2, 3, 5, 7, 8, 10 };
            case ScaleType::Dorian:     return { 0, 2, 3, 5, 7, 9, 10 };
            case ScaleType::Mixolydian: return { 0, 2, 4, 5, 7, 9, 10 };
            case ScaleType::Major:
            case ScaleType::Pentatonic:
            default:                    return { 0, 2, 4, 5, 7, 9, 11 };
        }
    }

    std::vector<int> melodyIntervals(ScaleType scale)
    {
        if (scale == ScaleType::Pentatonic)
            return { 0, 2, 4, 7, 9 };

        const auto mode = modeIntervals(scale);
        return { mode.begin(), mode.end() };
    }

    bool isMinorMode(ScaleType scale)
    {
        return scale == ScaleType::Minor || scale == ScaleType::Dorian;
    }

    std::string pitchClassName(int pitchClass, bool preferFlats)
    {
        static const char* sharps[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static const char* flats[]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        const int pc = ((pitchClass % 12) + 12) % 12;
        return preferFlats ? flats[pc] : sharps[pc];
    }

    bool keyPrefersFlats(int keyRoot, ScaleType scale)
    {
        // Relative-major tonic decides the key signature.
        const int majorTonic = isMinorMode(scale) ? (keyRoot + 3) % 12 : keyRoot;
        switch (majorTonic)
        {
            case 5: case 10: case 3: case 8: case 1: return true;  // F Bb Eb Ab Db
            default:                                 return false;
        }
    }

    ChordSpec parseChordToken(const std::string& token)
    {
        ChordSpec spec;
        size_t i = 0;

        if (i < token.size() && token[i] == 'b')
        {
            spec.borrowed = true;
            ++i;
        }

        if (i < token.size() && token[i] >= '1' && token[i] <= '7')
            spec.degree = token[i++] - '1';

        if (i < token.size() && token[i] == 'D')
        {
            spec.dominant = true;
            ++i;
        }

        const auto ext = token.substr(i);
        if (ext == "t")        spec.extension = 3;
        else if (ext == "7")   spec.extension = 7;
        else if (ext == "9")   spec.extension = 9;
        else if (ext == "11")  spec.extension = 11;
        else if (ext == "s2")  spec.suspension = 2;
        else if (ext == "s4")  spec.suspension = 4;
        else if (ext == "a9")  spec.add9 = true;

        return spec;
    }

    ChordInfo buildChord(int keyRoot, ScaleType scale, const ChordSpec& spec, int defaultExtension)
    {
        const auto mode = modeIntervals(scale);
        int extension = spec.extension != 0 ? spec.extension : defaultExtension;
        if (spec.suspension != 0 || spec.add9)
            extension = juce::jmin(extension, 7);

        ChordInfo chord;
        std::vector<int> intervals;

        if (spec.borrowed || spec.dominant)
        {
            static const std::array<int, 7> majorScale{ 0, 2, 4, 5, 7, 9, 11 };
            const int rootOffset = spec.borrowed ? majorScale[static_cast<size_t>(spec.degree)] - 1
                                                 : mode[static_cast<size_t>(spec.degree)];
            chord.rootPitchClass = (keyRoot + rootOffset + 12) % 12;

            intervals = { 0, 4, 7 };
            if (extension >= 7)
                intervals.push_back(spec.dominant || spec.degree == 6 ? 10 : 11);
            if (extension >= 9)
                intervals.push_back(spec.dominant && isMinorMode(scale) ? 13 : 14);  // V7b9 in minor
        }
        else
        {
            const int degree = spec.degree;
            chord.rootPitchClass = (keyRoot + mode[static_cast<size_t>(degree)]) % 12;

            const int numTones = extension >= 11 ? 6 : extension >= 9 ? 5 : extension >= 7 ? 4 : 3;
            for (int k = 0; k < numTones; ++k)
            {
                const int step = degree + 2 * k;
                const int interval = mode[static_cast<size_t>(step % 7)] + 12 * (step / 7)
                                     - mode[static_cast<size_t>(degree)];
                intervals.push_back(interval);
            }

            // Avoid harsh extensions: b9 on non-dominant chords, natural 11 against a major third.
            if (intervals.size() >= 5 && intervals[4] == 13)
                intervals.resize(4);
            if (intervals.size() >= 6 && intervals[1] == 4)
                intervals.resize(5);
        }

        if (spec.suspension != 0)
            intervals[1] = spec.suspension == 2 ? 2 : 5;

        if (spec.add9 && std::find(intervals.begin(), intervals.end(), 14) == intervals.end())
            intervals.push_back(14);

        std::sort(intervals.begin(), intervals.end());
        chord.intervals = intervals;
        chord.symbol = chordSymbol(chord.rootPitchClass, intervals,
                                   spec.borrowed || keyPrefersFlats(keyRoot, scale));
        return chord;
    }

    std::string chordSymbol(int rootPitchClass, const std::vector<int>& intervals, bool preferFlats)
    {
        std::set<int> tones;
        for (int interval : intervals)
            tones.insert(interval >= 12 ? interval : interval % 12);

        auto has = [&tones](int i) { return tones.count(i) > 0; };

        const bool majorThird = has(4);
        const bool minorThird = has(3);
        const bool sus2 = !majorThird && !minorThird && has(2);
        const bool sus4 = !majorThird && !minorThird && has(5);
        const bool dimFifth = has(6) && !has(7);
        const bool augFifth = has(8) && !has(7) && majorThird;
        const bool maj7 = has(11);
        const bool min7 = has(10);
        const bool dim7 = has(9) && dimFifth && minorThird && !min7;
        const bool ninth = has(14);
        const bool eleventh = has(17);

        std::string quality;

        if (sus2 || sus4)
        {
            quality = min7 ? (sus4 ? "7sus4" : "7sus2") : (sus4 ? "sus4" : "sus2");
            if (ninth && !min7)
                quality += "add9";
        }
        else if (minorThird && dimFifth)
        {
            quality = dim7 ? "dim7" : min7 ? "m7b5" : "dim";
        }
        else if (augFifth)
        {
            quality = "aug";
        }
        else if (minorThird)
        {
            if (maj7)              quality = "m(maj7)";
            else if (min7)         quality = eleventh ? "m11" : ninth ? "m9" : "m7";
            else                   quality = ninth ? "m(add9)" : "m";
        }
        else
        {
            if (maj7)              quality = ninth ? "maj9" : "maj7";
            else if (min7)         quality = ninth ? "9" : has(13) ? "7b9" : "7";
            else                   quality = ninth ? "add9" : "";
        }

        return pitchClassName(rootPitchClass, preferFlats) + quality;
    }

    std::vector<int> chordPitchClasses(const ChordInfo& chord)
    {
        std::vector<int> pcs;
        for (int interval : chord.intervals)
        {
            const int pc = (chord.rootPitchClass + interval) % 12;
            if (std::find(pcs.begin(), pcs.end(), pc) == pcs.end())
                pcs.push_back(pc);
        }
        return pcs;
    }

    bool containsPitchClass(const ChordInfo& chord, int pitch)
    {
        const auto pcs = chordPitchClasses(chord);
        return std::find(pcs.begin(), pcs.end(), ((pitch % 12) + 12) % 12) != pcs.end();
    }

    int nearestPitchWithClass(int pitchClass, int target)
    {
        const int pc = ((pitchClass % 12) + 12) % 12;
        int candidate = target - (((target % 12) - pc + 12) % 12);
        if (target - candidate > 6)
            candidate += 12;
        return candidate;
    }

    namespace
    {
        double movementCost(const std::vector<int>& from, const std::vector<int>& to)
        {
            double cost = 0.0;
            for (int note : to)
            {
                int best = 128;
                for (int prev : from)
                    best = juce::jmin(best, std::abs(note - prev));
                cost += best;
            }
            for (int prev : from)
            {
                int best = 128;
                for (int note : to)
                    best = juce::jmin(best, std::abs(note - prev));
                cost += best;
            }
            return cost;
        }
    }

    std::vector<int> voiceChord(const ChordInfo& chord, const std::vector<int>& previous,
                                const VoicingOptions& options, std::mt19937& rng)
    {
        // Ordered root, 3rd, 5th, 7th, 9th... as pitch classes
        std::vector<int> pcs;
        for (int interval : chord.intervals)
        {
            const int pc = (chord.rootPitchClass + interval) % 12;
            if (std::find(pcs.begin(), pcs.end(), pc) == pcs.end())
                pcs.push_back(pc);
        }

        const int fifthPc = (chord.rootPitchClass + 7) % 12;
        if (options.rootless && pcs.size() >= 4)
            pcs.erase(pcs.begin());
        while (static_cast<int>(pcs.size()) > options.maxNotes)
        {
            auto fifth = std::find(pcs.begin(), pcs.end(), fifthPc);
            if (fifth != pcs.end())
                pcs.erase(fifth);
            else
                pcs.erase(pcs.begin());
        }

        const int n = static_cast<int>(pcs.size());
        const double centre = (options.low + options.high) * 0.5;
        std::uniform_real_distribution<double> jitter(0.0, 0.75);

        std::vector<int> best;
        double bestScore = 1.0e9;

        auto consider = [&](std::vector<int> voicing, double penalty)
        {
            std::sort(voicing.begin(), voicing.end());
            if (voicing.front() < options.low || voicing.back() > options.high)
                return;

            // Close intervals low in the register sound muddy.
            for (size_t i = 1; i < voicing.size(); ++i)
                if (voicing[i - 1] < 52 && voicing[i] - voicing[i - 1] < 5)
                    penalty += 6.0;

            double mean = 0.0;
            for (int note : voicing)
                mean += note;
            mean /= static_cast<double>(voicing.size());

            double score = penalty + jitter(rng) + 0.35 * std::abs(mean - centre);
            if (!previous.empty())
                score += movementCost(previous, voicing)
                       + 0.5 * std::abs(voicing.back() - previous.back());

            if (score < bestScore)
            {
                bestScore = score;
                best = voicing;
            }
        };

        for (int inversion = 0; inversion < n; ++inversion)
        {
            for (int bottom = options.low; bottom < options.low + 12; ++bottom)
            {
                if (bottom % 12 != pcs[static_cast<size_t>(inversion)])
                    continue;

                std::vector<int> close{ bottom };
                for (int k = 1; k < n; ++k)
                {
                    const int pc = pcs[static_cast<size_t>((inversion + k) % n)];
                    int note = close.back() + 1;
                    while (note % 12 != pc)
                        ++note;
                    close.push_back(note);
                }

                for (int octaveShift = 0; octaveShift <= 12; octaveShift += 12)
                {
                    std::vector<int> shifted = close;
                    for (auto& note : shifted)
                        note += octaveShift;

                    const double inversionPenalty = previous.empty() && inversion != 0 ? 4.0 : 0.0;

                    if (options.open)
                    {
                        // Spread: raise every second voice by an octave.
                        std::vector<int> spread = shifted;
                        for (size_t k = 1; k < spread.size(); k += 2)
                            spread[k] += 12;
                        consider(spread, inversionPenalty);
                        consider(shifted, inversionPenalty + 3.0);
                    }
                    else
                    {
                        consider(shifted, inversionPenalty);
                        if (n >= 4)
                        {
                            std::vector<int> drop2 = shifted;
                            std::sort(drop2.begin(), drop2.end());
                            drop2[drop2.size() - 2] -= 12;
                            consider(drop2, inversionPenalty + 0.5);
                        }
                    }
                }
            }
        }

        if (best.empty())
        {
            for (int pc : pcs)
                best.push_back(nearestPitchWithClass(pc, static_cast<int>(centre)));
            std::sort(best.begin(), best.end());
        }

        return best;
    }
}
