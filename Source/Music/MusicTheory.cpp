#include "MusicTheory.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>
#include <sstream>

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
        else if (ext == "h" || ext == "ø")
        {
            spec.halfDiminished = true;
            spec.extension = 7;
        }

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

        if (spec.halfDiminished)
        {
            const int degree = spec.degree;
            chord.rootPitchClass = (keyRoot + mode[static_cast<size_t>(degree)]) % 12;
            intervals = { 0, 3, 6, 10 };
        }
        else if (spec.borrowed || spec.dominant)
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
            // Minor-second clusters (9th against a minor 3rd, 11th against a 5th).
            for (size_t i = 0; i < voicing.size(); ++i)
                for (size_t j = i + 1; j < voicing.size(); ++j)
                    if (std::abs(voicing[j] - voicing[i]) % 12 == 1)
                        penalty += 9.0;
            if (!voicing.empty() && voicing.front() % 12 == chord.rootPitchClass
                && voicing.back() - voicing.front() < 10)
                penalty += 2.8;

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
                    // Keep 9ths/11ths off the adjacent minor 2nd by lifting them.
                    if (note - close.back() == 1 && note + 12 <= options.high + 12)
                        note += 12;
                    close.push_back(note);
                }

                for (int octaveShift = 0; octaveShift <= 12; octaveShift += 12)
                {
                    std::vector<int> shifted = close;
                    for (auto& note : shifted)
                        note += octaveShift;

                    // Prefer inverted / drop-2 voicings over stacked root-position triads.
                    const double inversionPenalty = inversion == 0
                        ? (previous.empty() ? 1.2 : 3.4)
                        : (inversion == 1 ? 0.0 : 0.45);

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

    namespace
    {
        std::string trimCopy(std::string s)
        {
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
                s.erase(s.begin());
            while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
                s.pop_back();
            return s;
        }

        std::string toLowerCopy(std::string s)
        {
            for (auto& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        }

        bool looksRoman(const std::string& token)
        {
            size_t i = 0;
            if (!token.empty() && (token[0] == 'b' || token[0] == '#'))
                ++i;
            if (i >= token.size())
                return false;
            const char c = token[i];
            return c == 'I' || c == 'V' || c == 'i' || c == 'v';
        }

        bool consumeRoman(const std::string& token, size_t& i, int& degree, bool& minorCase)
        {
            const auto match = [&](const char* upper, const char* lower, int deg) -> bool
            {
                const size_t n = std::strlen(upper);
                if (token.size() - i >= n && token.compare(i, n, upper) == 0)
                {
                    degree = deg;
                    minorCase = false;
                    i += n;
                    return true;
                }
                if (token.size() - i >= n && token.compare(i, n, lower) == 0)
                {
                    degree = deg;
                    minorCase = true;
                    i += n;
                    return true;
                }
                return false;
            };

            return match("VII", "vii", 6) || match("III", "iii", 2) || match("II", "ii", 1)
                || match("IV", "iv", 3) || match("VI", "vi", 5) || match("I", "i", 0)
                || match("V", "v", 4);
        }

        std::vector<int> intervalsFromQuality(std::string quality)
        {
            quality = toLowerCopy(quality);
            quality.erase(std::remove_if(quality.begin(), quality.end(),
                                         [](char c) { return c == '(' || c == ')' || c == '^'; }),
                          quality.end());

            if (quality == "m9" || quality == "min9" || quality == "-9")         return { 0, 3, 7, 10, 14 };
            if (quality == "m11" || quality == "min11")                          return { 0, 3, 7, 10, 14, 17 };
            if (quality == "m7" || quality == "min7" || quality == "-7")         return { 0, 3, 7, 10 };
            if (quality == "mmaj7" || quality == "m(maj7)" || quality == "minmaj7") return { 0, 3, 7, 11 };
            if (quality == "m6" || quality == "min6")                            return { 0, 3, 7, 9 };
            if (quality == "m" || quality == "min" || quality == "-")            return { 0, 3, 7 };
            if (quality == "maj9" || quality == "m9j" || quality == "Δ9")        return { 0, 4, 7, 11, 14 };
            if (quality == "maj7" || quality == "ma7" || quality == "Δ7")        return { 0, 4, 7, 11 };
            if (quality == "maj")                                                return { 0, 4, 7 };
            if (quality == "7b9")                                                return { 0, 4, 7, 10, 13 };
            if (quality == "7#9")                                                return { 0, 4, 7, 10, 15 };
            if (quality == "9")                                                  return { 0, 4, 7, 10, 14 };
            if (quality == "11")                                                 return { 0, 4, 7, 10, 14, 17 };
            if (quality == "13")                                                 return { 0, 4, 7, 10, 14, 21 };
            if (quality == "7sus4")                                              return { 0, 5, 7, 10 };
            if (quality == "7")                                                  return { 0, 4, 7, 10 };
            if (quality == "dim7" || quality == "o7")                            return { 0, 3, 6, 9 };
            if (quality == "dim" || quality == "o" || quality == "mb5")          return { 0, 3, 6 };
            if (quality == "m7b5" || quality == "ø")                             return { 0, 3, 6, 10 };
            if (quality == "aug" || quality == "+")                              return { 0, 4, 8 };
            if (quality == "sus4" || quality == "sus")                           return { 0, 5, 7 };
            if (quality == "sus2")                                               return { 0, 2, 7 };
            if (quality == "add9")                                               return { 0, 4, 7, 14 };
            if (quality == "6")                                                  return { 0, 4, 7, 9 };
            if (quality == "5")                                                  return { 0, 7 };
            return {};
        }
    }

    bool parseUserChord(const std::string& rawToken, int keyRoot, ScaleType scale,
                        int defaultExtension, ChordInfo& out)
    {
        const std::string token = trimCopy(rawToken);
        if (token.empty())
            return false;

        if (looksRoman(token))
        {
            size_t i = 0;
            bool borrowed = false;
            if (token[0] == 'b') { borrowed = true; ++i; }
            else if (token[0] == '#') { ++i; }

            int degree = 0;
            bool minorCase = false;
            if (!consumeRoman(token, i, degree, minorCase))
                return false;

            const std::string quality = token.substr(i);
            if (quality.empty())
            {
                ChordSpec spec;
                spec.degree = degree;
                spec.borrowed = borrowed;
                if (minorCase)
                    spec.extension = 3;
                out = buildChord(keyRoot, scale, spec, minorCase ? 3 : defaultExtension);
                return true;
            }

            const auto intervals = intervalsFromQuality(quality);
            if (intervals.empty())
                return false;

            const auto mode = modeIntervals(scale);
            int rootOffset = mode[static_cast<size_t>(degree)];
            if (borrowed)
            {
                static const std::array<int, 7> majorScale{ 0, 2, 4, 5, 7, 9, 11 };
                rootOffset = (majorScale[static_cast<size_t>(degree)] - 1 + 12) % 12;
            }

            out.rootPitchClass = (keyRoot + rootOffset + 12) % 12;
            out.intervals = intervals;
            out.symbol = chordSymbol(out.rootPitchClass, intervals,
                                     borrowed || keyPrefersFlats(keyRoot, scale));
            return true;
        }

        size_t i = 0;
        if (i >= token.size())
            return false;
        const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(token[i])));
        static const int letterPc[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
        if (letter < 'A' || letter > 'G')
            return false;
        int pc = letterPc[letter - 'A'];
        ++i;
        if (i < token.size() && (token[i] == '#' || token[i] == 'b'))
        {
            pc = (pc + (token[i] == '#' ? 1 : -1) + 12) % 12;
            ++i;
        }

        std::string quality = token.substr(i);
        auto intervals = intervalsFromQuality(quality);
        if (intervals.empty())
        {
            if (quality.empty())
                intervals = { 0, 4, 7 };
            else
                return false;
        }

        out.rootPitchClass = pc;
        out.intervals = intervals;
        out.symbol = chordSymbol(pc, intervals, keyPrefersFlats(keyRoot, scale) || token.find('b') != std::string::npos);
        return true;
    }

    std::vector<ChordInfo> parseProgressionText(const std::string& text, int keyRoot,
                                                ScaleType scale, int bars, int defaultExtension)
    {
        std::vector<ChordInfo> parsed;
        const int safeBars = juce::jmax(1, bars);
        const std::string input = trimCopy(text);
        if (input.empty())
            return parsed;

        std::vector<std::vector<std::string>> barsTokens;
        if (input.find('|') != std::string::npos)
        {
            std::istringstream barStream(input);
            for (std::string bar; std::getline(barStream, bar, '|');)
            {
                std::vector<std::string> tokens;
                std::istringstream tokStream(bar);
                for (std::string tok; tokStream >> tok;)
                    tokens.push_back(tok);
                if (!tokens.empty())
                    barsTokens.push_back(std::move(tokens));
            }
        }
        else
        {
            std::istringstream tokStream(input);
            for (std::string tok; tokStream >> tok;)
                barsTokens.push_back({ tok });
        }

        if (barsTokens.empty())
            return parsed;

        for (int bar = 0; bar < safeBars; ++bar)
        {
            const auto& tokens = barsTokens[static_cast<size_t>(bar) % barsTokens.size()];
            const double barStart = bar * 4.0;
            const double each = 4.0 / static_cast<double>(tokens.size());
            for (size_t i = 0; i < tokens.size(); ++i)
            {
                ChordInfo chord;
                if (!parseUserChord(tokens[i], keyRoot, scale, defaultExtension, chord))
                    continue;
                chord.startBeat = barStart + static_cast<double>(i) * each;
                chord.lengthBeats = each;
                parsed.push_back(std::move(chord));
            }
        }

        return parsed;
    }
}
