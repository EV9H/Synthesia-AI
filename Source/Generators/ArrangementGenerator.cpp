#include "ArrangementGenerator.h"
#include "ArrangementPlan.h"
#include "../StyleProfile.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <map>
#include <sstream>
#include <utility>

namespace
{
    constexpr double stepBeats = 0.25;
    constexpr double ticksPerBeat = 960.0;

    using Notes = ArrangementGenerator::PartNotes;

    int gridSteps(Quantization quantization)
    {
        switch (quantization)
        {
            case Quantization::Sixteenth: return 1;
            case Quantization::Eighth:    return 2;
            case Quantization::Quarter:   return 4;
            case Quantization::Half:      return 8;
        }
        return 2;
    }

    double uniform(std::mt19937& rng)
    {
        return std::uniform_real_distribution<double>(0.0, 1.0)(rng);
    }

    template <typename T>
    const T& pick(const std::vector<T>& items, std::mt19937& rng)
    {
        return items[rng() % items.size()];
    }

    int humanise(int velocity, std::mt19937& rng, int amount = 6)
    {
        const int offset = static_cast<int>(rng() % static_cast<unsigned>(2 * amount + 1)) - amount;
        return juce::jlimit(1, 127, velocity + offset);
    }

    int playedVelocity(int base, double start, std::mt19937& rng, int spread)
    {
        const double barBeat = start - 4.0 * std::floor(start / 4.0 + 1.0e-9);
        const bool downbeat = barBeat < 0.14;
        const bool strong = downbeat || (barBeat > 1.86 && barBeat < 2.14);
        const int accent = downbeat ? 10 : strong ? 4 : -7;
        return humanise(base + accent, rng, spread);
    }

    void dropMinorSeconds(std::vector<int>& voicing)
    {
        if (voicing.size() < 2)
            return;
        std::sort(voicing.begin(), voicing.end());
        std::vector<int> kept;
        kept.push_back(voicing.front());
        for (size_t i = 1; i < voicing.size(); ++i)
        {
            if (std::abs(voicing[i] - kept.back()) % 12 == 1)
                continue; // drop the inner clash, keep the lower tone
            kept.push_back(voicing[i]);
        }
        if (kept.size() >= 2)
            voicing = std::move(kept);
    }

    std::vector<int> invertDiatonic(std::vector<int> voicing, const theory::VoicingOptions& options)
    {
        if (voicing.size() < 3)
            return voicing;
        std::rotate(voicing.begin(), voicing.begin() + 1, voicing.end());
        if (voicing.back() + 12 <= options.high)
            voicing.back() += 12;
        else if (voicing.front() - 12 >= options.low)
            voicing.front() -= 12;
        std::sort(voicing.begin(), voicing.end());
        dropMinorSeconds(voicing);
        return voicing;
    }

    std::mt19937 streamFor(unsigned int seed, unsigned int stream)
    {
        std::seed_seq sequence{ seed, stream, 0x5eedu };
        return std::mt19937(sequence);
    }

    int chordIndexAt(const std::vector<ChordInfo>& progression, double beat)
    {
        for (int i = static_cast<int>(progression.size()) - 1; i >= 0; --i)
            if (beat + 1.0e-6 >= progression[static_cast<size_t>(i)].startBeat)
                return i;
        return 0;
    }

    double chordEnd(const ChordInfo& chord)
    {
        return chord.startBeat + chord.lengthBeats;
    }

    int foldIntoRange(int pitch, int low, int high)
    {
        while (pitch < low)
            pitch += 12;
        while (pitch > high && pitch - 12 >= low)
            pitch -= 12;
        return pitch;
    }

    struct Hit
    {
        int step;
        int length;
        char symbol;
    };

    std::vector<Hit> parsePattern(const std::string& pattern)
    {
        std::vector<Hit> hits;
        const int size = static_cast<int>(pattern.size());
        for (int i = 0; i < size; ++i)
        {
            const char c = pattern[static_cast<size_t>(i)];
            if (c == '-' || c == '.')
                continue;

            int length = 1;
            while (i + length < size && pattern[static_cast<size_t>(i + length)] == '-')
                ++length;
            hits.push_back({ i, length, c });
        }
        return hits;
    }

    std::vector<int> scalePitches(int keyRoot, ScaleType scale, int low, int high)
    {
        const auto intervals = theory::melodyIntervals(scale);
        std::vector<int> pitches;
        for (int pitch = juce::jmax(0, low); pitch <= juce::jmin(127, high); ++pitch)
        {
            const int relative = ((pitch - keyRoot) % 12 + 12) % 12;
            if (std::find(intervals.begin(), intervals.end(), relative) != intervals.end())
                pitches.push_back(pitch);
        }
        return pitches;
    }

    std::vector<int> chordTonePitches(const ChordInfo& chord, bool withExtensions, int low, int high)
    {
        std::vector<int> pcs;
        const size_t count = withExtensions ? chord.intervals.size()
                                            : juce::jmin<size_t>(3, chord.intervals.size());
        for (size_t i = 0; i < count; ++i)
            pcs.push_back((chord.rootPitchClass + chord.intervals[i]) % 12);

        std::vector<int> pitches;
        for (int pitch = juce::jmax(0, low); pitch <= juce::jmin(127, high); ++pitch)
            if (std::find(pcs.begin(), pcs.end(), pitch % 12) != pcs.end())
                pitches.push_back(pitch);
        return pitches;
    }

    // Key scale adjusted to the chord: scale notes a semitone from a chromatic chord tone
    // take that chord tone (e.g. G# over E7 in A minor, Ab over a borrowed bVI).
    std::vector<int> chordScale(const std::vector<int>& scale, const ChordInfo& chord,
                                int keyRoot, ScaleType scaleType)
    {
        const auto keyIntervals = theory::melodyIntervals(scaleType);
        auto inKey = [&](int pc)
        {
            return std::find(keyIntervals.begin(), keyIntervals.end(), ((pc - keyRoot) % 12 + 12) % 12)
                   != keyIntervals.end();
        };

        std::vector<int> chromaticTones;
        for (int pc : theory::chordPitchClasses(chord))
            if (!inKey(pc))
                chromaticTones.push_back(pc);

        if (chromaticTones.empty())
            return scale;

        std::vector<int> adjusted;
        for (int pitch : scale)
        {
            int replacement = pitch;
            for (int pc : chromaticTones)
            {
                if ((pitch + 1) % 12 == pc)       replacement = pitch + 1;
                else if ((pitch + 11) % 12 == pc) replacement = pitch - 1;
            }
            if (theory::containsPitchClass(chord, pitch))
                replacement = pitch;
            adjusted.push_back(replacement);
        }

        std::sort(adjusted.begin(), adjusted.end());
        adjusted.erase(std::unique(adjusted.begin(), adjusted.end()), adjusted.end());
        return adjusted;
    }

    bool notesOverlap(const ArrangementGenerator::PartNote& a, const ArrangementGenerator::PartNote& b)
    {
        return a.start < b.start + b.length - 0.03 && b.start < a.start + a.length - 0.03;
    }

    void snapBassToHarmony(Notes& bass, const std::vector<ChordInfo>& progression, int low, int high)
    {
        for (auto& note : bass)
        {
            const double barBeat = note.start - 4.0 * std::floor(note.start / 4.0 + 1.0e-9);
            const bool strong = barBeat < 0.14 || (barBeat > 1.86 && barBeat < 2.14);
            if (!strong)
                continue;
            const auto& chord = progression[static_cast<size_t>(chordIndexAt(progression, note.start))];
            const int rootPc = chord.rootPitchClass;
            const int fifthPc = (rootPc + 7) % 12;
            const int pc = ((note.pitch % 12) + 12) % 12;
            if (pc == rootPc || pc == fifthPc)
                continue;
            note.pitch = foldIntoRange(theory::nearestPitchWithClass(rootPc, note.pitch), low, high);
        }
    }

    bool pitchInKey(int pitch, int keyRoot, ScaleType scaleType)
    {
        const auto intervals = theory::melodyIntervals(scaleType);
        const int relative = ((pitch - keyRoot) % 12 + 12) % 12;
        return std::find(intervals.begin(), intervals.end(), relative) != intervals.end();
    }

    bool pitchAllowed(int pitch, const ChordInfo& chord, int keyRoot, ScaleType scaleType)
    {
        return pitchInKey(pitch, keyRoot, scaleType) || theory::containsPitchClass(chord, pitch);
    }

    int snapToPitchClasses(int pitch, const std::vector<int>& pcs, int lo, int hi)
    {
        if (pcs.empty())
            return juce::jlimit(lo, hi, pitch);
        int best = foldIntoRange(theory::nearestPitchWithClass(pcs.front(), pitch), lo, hi);
        int bestDistance = std::abs(best - pitch);
        for (int pc : pcs)
        {
            const int candidate = foldIntoRange(theory::nearestPitchWithClass(pc, pitch), lo, hi);
            const int distance = std::abs(candidate - pitch);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
        return best;
    }

    void resolveUnisons(std::map<Part, Notes>& parts, const MidiGenerationParams& params,
                        const std::vector<ChordInfo>& progression)
    {
        auto collide = [&](Part moving, int lo, int hi, int midLo, int midHi)
        {
            auto mit = parts.find(moving);
            if (mit == parts.end())
                return;
            for (auto& a : mit->second)
            {
                if (a.pitch < midLo || a.pitch > midHi)
                    continue;
                auto occupied = [&](int pitch)
                {
                    for (const auto& [part, notes] : parts)
                    {
                        if (part == moving || part == Part::Drums)
                            continue;
                        // Piano doubling a chord stab is expected (lint skips it).
                        if ((moving == Part::Piano && part == Part::Chords)
                            || (moving == Part::Chords && part == Part::Piano))
                            continue;
                        for (const auto& b : notes)
                            if (b.pitch == pitch && notesOverlap(a, b))
                                return true;
                    }
                    return false;
                };
                if (!occupied(a.pitch))
                    continue;

                const auto& chord = progression[static_cast<size_t>(chordIndexAt(progression, a.start))];
                auto allowed = [&](int pitch)
                {
                    return pitchAllowed(pitch, chord, params.rootNote, params.scale);
                };
                auto isChordTone = [&](int pitch)
                {
                    return theory::containsPitchClass(chord, pitch);
                };

                // Prefer a nearby chord tone (third / fourth / fifth / octave).
                // Never land on a pitch outside the key unless it is a borrowed chord tone.
                const std::initializer_list<int> downFirst{ -12, -7, -5, -4, -3, 12, 7, 5, 4, 3, -8, 8, -9, 9 };
                const std::initializer_list<int> upFirst{ 12, 7, 5, 4, 3, -12, -7, -5, -4, -3, 8, -8, 9, -9 };
                bool moved = false;
                for (int pass = 0; pass < 2 && !moved; ++pass)
                {
                    const bool chordPass = pass == 0;
                    for (int delta : a.pitch >= 68 ? downFirst : upFirst)
                    {
                        const int candidate = a.pitch + delta;
                        if (candidate < lo || candidate > hi || occupied(candidate) || !allowed(candidate))
                            continue;
                        if (chordPass && !isChordTone(candidate))
                            continue;
                        a.pitch = candidate;
                        moved = true;
                        break;
                    }
                }
                if (!moved)
                {
                    for (int d = 1; d <= 16 && !moved; ++d)
                    {
                        for (int sign : { -1, 1 })
                        {
                            const int candidate = a.pitch + sign * d;
                            if (candidate >= lo && candidate <= hi && !occupied(candidate)
                                && allowed(candidate))
                            {
                                a.pitch = candidate;
                                moved = true;
                                break;
                            }
                        }
                    }
                }
                if (!moved)
                    a.length = 0.0; // drop the doubling rather than leave a unison
            }
            mit->second.erase(std::remove_if(mit->second.begin(), mit->second.end(),
                                             [](const ArrangementGenerator::PartNote& n)
                                             { return n.length < 0.02; }),
                              mit->second.end());
        };

        for (int pass = 0; pass < 4; ++pass)
        {
            // Move the lead off pads/piano/bass. Piano doubling chords is allowed.
            collide(Part::Synth, 60, 84, 40, 90);
            collide(Part::Chords, 48, 88, 36, 60); // only vs bass in the low mid
            collide(Part::Piano, 52, 96, 26, 55);  // only vs bass
        }
    }

    void enforceKey(std::map<Part, Notes>& parts, const MidiGenerationParams& params,
                    const std::vector<ChordInfo>& progression)
    {
        if (progression.empty())
            return;

        const std::map<Part, std::pair<int, int>> ranges{
            { Part::Chords, { 48, 88 } },
            { Part::Bass,   { 24, 60 } },
            { Part::Synth,  { 55, 88 } },
            { Part::Piano,  { 48, 96 } },
        };

        for (auto& [part, notes] : parts)
        {
            if (part == Part::Drums || notes.empty())
                continue;
            const auto range = ranges.count(part) != 0 ? ranges.at(part) : std::pair<int, int>{ 24, 96 };

            std::vector<size_t> order(notes.size());
            for (size_t i = 0; i < order.size(); ++i)
                order[i] = i;
            std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
            {
                return notes[a].start < notes[b].start;
            });

            for (size_t oi = 0; oi < order.size(); ++oi)
            {
                auto& note = notes[order[oi]];
                const auto& chord = progression[static_cast<size_t>(chordIndexAt(progression, note.start))];
                const bool inChord = theory::containsPitchClass(chord, note.pitch);
                const bool inKey = pitchInKey(note.pitch, params.rootNote, params.scale);
                const double barBeat = note.start - 4.0 * std::floor(note.start / 4.0 + 1.0e-9);
                const bool strong = barBeat < 0.14 || (barBeat > 1.86 && barBeat < 2.14);

                if (!inKey && !inChord && !strong && oi + 1 < order.size())
                {
                    const double durSec = note.length * 60.0 / juce::jmax(1.0, params.tempo);
                    const auto& next = notes[order[oi + 1]];
                    const int step = std::abs(next.pitch - note.pitch);
                    const bool nextLegal = pitchAllowed(next.pitch,
                        progression[static_cast<size_t>(chordIndexAt(progression, next.start))],
                        params.rootNote, params.scale);
                    // Off-beat chromatic passing tone: short, resolves by step.
                    if (durSec < 0.35 && step >= 1 && step <= 2 && nextLegal)
                        continue;
                }

                if (strong && !inChord)
                {
                    const auto pcs = theory::chordPitchClasses(chord);
                    if (!pcs.empty())
                        note.pitch = snapToPitchClasses(note.pitch, pcs, range.first, range.second);
                    continue;
                }

                if (!inKey && !inChord)
                {
                    std::vector<int> pcs = theory::chordPitchClasses(chord);
                    for (int interval : theory::melodyIntervals(params.scale))
                        pcs.push_back((params.rootNote + interval) % 12);
                    note.pitch = snapToPitchClasses(note.pitch, pcs, range.first, range.second);
                }
            }
        }
    }

    int scaleMove(const std::vector<int>& scale, int pitch, int delta);

    void addInnerVoiceMotion(Notes& notes, const std::vector<int>& scale, std::mt19937& rng)
    {
        if (notes.size() < 3)
            return;

        std::vector<size_t> order(notes.size());
        for (size_t i = 0; i < order.size(); ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b)
        {
            return notes[a].start != notes[b].start ? notes[a].start < notes[b].start
                                                    : notes[a].pitch < notes[b].pitch;
        });

        Notes extras;
        for (size_t i = 0; i < order.size();)
        {
            const double start = notes[order[i]].start;
            std::vector<size_t> stack;
            size_t j = i;
            while (j < order.size() && std::abs(notes[order[j]].start - start) < 0.06)
            {
                stack.push_back(order[j]);
                ++j;
            }
            i = j;
            if (stack.size() < 3)
                continue;

            double hold = notes[stack.front()].length;
            for (size_t idx : stack)
                hold = juce::jmin(hold, notes[idx].length);
            if (hold < 1.45)
                continue;

            const size_t innerIdx = stack[stack.size() - 2];
            auto& inner = notes[innerIdx];
            const double tail = start + hold;
            int currentPitch = inner.pitch;
            int dir = uniform(rng) < 0.5 ? 1 : -1;
            bool first = true;
            for (double motionAt = start + 1.0; motionAt < tail - 0.35; motionAt += 1.0)
            {
                int moved = scaleMove(scale, currentPitch, dir);
                if (moved == currentPitch)
                    moved = scaleMove(scale, currentPitch, -dir);
                dir = -dir;

                bool clash = false;
                for (size_t idx : stack)
                {
                    if (idx == innerIdx)
                        continue;
                    const int other = notes[idx].pitch;
                    if (other == moved || std::abs(other - moved) % 12 == 1)
                        clash = true;
                }
                if (clash)
                {
                    int alt = scaleMove(scale, currentPitch, moved >= currentPitch ? 2 : -2);
                    bool altClash = alt == currentPitch;
                    for (size_t idx : stack)
                    {
                        if (idx == innerIdx)
                            continue;
                        const int other = notes[idx].pitch;
                        if (other == alt || std::abs(other - alt) % 12 == 1)
                            altClash = true;
                    }
                    if (altClash)
                        continue;
                    moved = alt;
                }

                if (first)
                {
                    inner.length = juce::jmax(0.2, motionAt - start - 0.02);
                    first = false;
                }
                extras.push_back({ moved, motionAt,
                                   juce::jmax(0.35, juce::jmin(1.0, tail - motionAt) - 0.02),
                                   juce::jlimit(1, 127, inner.velocity - 4) });
                currentPitch = moved;
            }
        }

        notes.insert(notes.end(), extras.begin(), extras.end());
    }

    int nearestOf(const std::vector<int>& candidates, int target, int fallback)
    {
        int best = fallback;
        int bestDistance = 1000;
        for (int candidate : candidates)
        {
            const int distance = std::abs(candidate - target);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }
        return best;
    }

    int scaleMove(const std::vector<int>& scale, int pitch, int delta)
    {
        if (scale.empty())
            return pitch;

        const auto it = std::lower_bound(scale.begin(), scale.end(), pitch);
        int index = static_cast<int>(std::distance(scale.begin(), it));
        if (index >= static_cast<int>(scale.size()) || (index > 0 && scale[static_cast<size_t>(index)] != pitch
                && pitch - scale[static_cast<size_t>(index - 1)] < scale[static_cast<size_t>(index)] - pitch))
            index = juce::jmax(0, index - 1);

        index = juce::jlimit(0, static_cast<int>(scale.size()) - 1, index + delta);
        return scale[static_cast<size_t>(index)];
    }
}

juce::MidiBuffer ArrangementGenerator::generate(const MidiGenerationParams& params)
{
    return generateArrangement(params).midi;
}

std::vector<ChordInfo> ArrangementGenerator::buildProgression(const MidiGenerationParams& params)
{
    const unsigned int seed = params.seed != 0 ? params.seed : std::random_device{}();
    auto rng = streamFor(seed, 1);
    return buildProgression(params, rng);
}

std::vector<ChordInfo> ArrangementGenerator::buildProgression(const MidiGenerationParams& params,
                                                              std::mt19937& rng)
{
    if (params.styleBlend > 0.45 && !params.styleProfileJson.empty())
    {
        const auto style = StyleProfile::fromJson(juce::String(params.styleProfileJson));
        auto sampled = StyleProfile::sampleProgression(style, params, rng);
        if (!sampled.empty())
            return sampled;
    }

    const auto profile = StyleProfile::effectiveGenre(params);
    const auto& templates = theory::isMinorMode(params.scale) ? profile.minorProgressions
                                                              : profile.majorProgressions;
    const auto& chosen = pick(templates, rng);

    std::vector<std::string> tokens;
    std::istringstream stream(chosen.tokens);
    for (std::string token; stream >> token;)
        tokens.push_back(token);

    std::vector<ChordInfo> progression;
    const double totalBeats = params.lengthInBars * 4.0;
    const double beatsPerChord = chosen.barsPerChord * 4.0;

    size_t index = 0;
    for (double beat = 0.0; beat < totalBeats - 1.0e-6; beat += beatsPerChord, ++index)
    {
        const auto spec = theory::parseChordToken(tokens[index % tokens.size()]);
        auto chord = theory::buildChord(params.rootNote, params.scale, spec, profile.defaultExtension);
        chord.startBeat = beat;
        chord.lengthBeats = juce::jmin(beatsPerChord, totalBeats - beat);
        progression.push_back(chord);
    }

    return progression;
}

const SectionInfo* ArrangementGenerator::Context::sectionAt(double beat) const
{
    for (const auto& section : sections)
        if (beat + 1.0e-6 >= section.startBeat && beat < section.startBeat + section.lengthBeats)
            return &section;
    return sections.empty() ? nullptr : &sections.back();
}

float ArrangementGenerator::Context::energyAt(double beat) const
{
    if (const auto* section = sectionAt(beat))
        return section->energy;
    return 0.7f;
}

float ArrangementGenerator::Context::densityAt(Part part, double beat) const
{
    const auto* section = sectionAt(beat);
    float d = 0.7f;
    if (section != nullptr)
    {
        switch (part)
        {
            case Part::Chords: d = section->densityChords; break;
            case Part::Bass:   d = section->densityBass; break;
            case Part::Synth:  d = section->densitySynth; break;
            case Part::Piano:  d = section->densityPiano; break;
            case Part::Drums:  d = section->densityDrums; break;
            case Part::All:    d = section->energy; break;
        }
    }
    if (params.density > 0.01f)
        d = juce::jlimit(0.05f, 1.0f, 0.4f * d + 0.6f * params.density);
    return d;
}

namespace
{
    float energyForKind(SectionKind kind)
    {
        switch (kind)
        {
            case SectionKind::Intro:     return 0.32f;
            case SectionKind::Verse:     return 0.42f;
            case SectionKind::PreChorus: return 0.78f;
            case SectionKind::Chorus:    return 1.00f;
            case SectionKind::Drop:      return 1.00f;
            case SectionKind::Bridge:    return 0.52f;
            case SectionKind::Break:     return 0.28f;
            case SectionKind::Outro:     return 0.40f;
        }
        return 0.7f;
    }

    using FormSpec = std::pair<SectionKind, int>;

    std::vector<FormSpec> formTemplate(Genre genre, int bars)
    {
        const bool longForm = bars >= 28;

        switch (genre)
        {
            case Genre::House:
                if (longForm)
                    return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                             { SectionKind::PreChorus, 4 }, { SectionKind::Drop, 8 },
                             { SectionKind::Break, 4 }, { SectionKind::Drop, 4 } };
                if (bars >= 20)
                    return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                             { SectionKind::Drop, 8 }, { SectionKind::Outro, 4 } };
                return { { SectionKind::Intro, 4 }, { SectionKind::Drop, 8 }, { SectionKind::Outro, 4 } };

            case Genre::Trap:
                if (longForm)
                    return { { SectionKind::Intro, 8 }, { SectionKind::Verse, 8 },
                             { SectionKind::Drop, 8 }, { SectionKind::Bridge, 4 },
                             { SectionKind::Drop, 4 } };
                if (bars >= 20)
                    return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                             { SectionKind::Drop, 8 }, { SectionKind::Outro, 4 } };
                return { { SectionKind::Intro, 4 }, { SectionKind::Drop, 8 }, { SectionKind::Outro, 4 } };

            case Genre::LoFiJazz:
                if (longForm)
                    return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                             { SectionKind::Chorus, 8 }, { SectionKind::Verse, 8 },
                             { SectionKind::Outro, 4 } };
                return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::Chorus, 8 }, { SectionKind::Outro, 4 } };

            case Genre::Pop:
                if (longForm)
                    return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                             { SectionKind::PreChorus, 4 }, { SectionKind::Chorus, 8 },
                             { SectionKind::Bridge, 4 }, { SectionKind::Chorus, 4 } };
                return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::Chorus, 8 }, { SectionKind::Outro, 4 } };

            case Genre::RnB:
                if (longForm)
                    return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                             { SectionKind::PreChorus, 4 }, { SectionKind::Chorus, 8 },
                             { SectionKind::Bridge, 4 }, { SectionKind::Chorus, 4 } };
                return { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::Chorus, 8 }, { SectionKind::Outro, 4 } };

            case Genre::Cinematic:
                if (longForm)
                    return { { SectionKind::Intro, 8 }, { SectionKind::Verse, 8 },
                             { SectionKind::Chorus, 8 }, { SectionKind::Bridge, 4 },
                             { SectionKind::Outro, 4 } };
                return { { SectionKind::Intro, 8 }, { SectionKind::Verse, 8 }, { SectionKind::Outro, 8 } };
        }

        return { { SectionKind::Verse, bars } };
    }
}

namespace
{
    float densityFor(SectionKind kind, Part part)
    {
        const float energy = energyForKind(kind);
        switch (part)
        {
            case Part::Drums:  return kind == SectionKind::Break ? 0.18f
                                 : kind == SectionKind::Intro ? 0.42f : energy;
            case Part::Synth:  return kind == SectionKind::Intro ? 0.18f
                                 : kind == SectionKind::Verse ? 0.38f
                                 : kind == SectionKind::Break ? 0.22f : energy;
            case Part::Piano:  return kind == SectionKind::Drop ? 0.28f
                                 : kind == SectionKind::Chorus ? 0.40f
                                 : kind == SectionKind::Intro ? 0.55f
                                 : juce::jmin(1.0f, energy + 0.05f);
            case Part::Bass:   return kind == SectionKind::Intro ? 0.35f
                                 : kind == SectionKind::Break ? 0.30f
                                 : juce::jmax(0.45f, energy);
            case Part::Chords: return kind == SectionKind::Break ? 0.35f
                                 : kind == SectionKind::Intro ? 0.55f : energy;
            case Part::All:    return energy;
        }
        return energy;
    }

    SectionInfo makeSection(SectionKind kind, double startBeat, int bars, int occurrence)
    {
        std::string label = sectionName(kind);
        if (occurrence > 1)
            label += " " + std::to_string(occurrence);
        float energy = energyForKind(kind);
        if (occurrence > 1)
            energy = juce::jmin(1.0f, energy + 0.06f);
        return { kind, startBeat, bars * 4.0, energy, occurrence, label,
                 densityFor(kind, Part::Chords), densityFor(kind, Part::Bass),
                 densityFor(kind, Part::Synth), densityFor(kind, Part::Piano),
                 densityFor(kind, Part::Drums) };
    }
}

std::vector<SectionInfo> ArrangementGenerator::buildForm(Genre genre, ArrangementForm form, int bars)
{
    const int safeBars = juce::jmax(1, bars);
    if (form != ArrangementForm::Song || safeBars < 8)
    {
        if (safeBars >= 8)
        {
            const int first = safeBars / 2;
            const int second = safeBars - first;
            return {
                makeSection(SectionKind::Verse, 0.0, first, 1),
                makeSection(SectionKind::Chorus, first * 4.0, second, 1)
            };
        }
        return { makeSection(SectionKind::Verse, 0.0, safeBars, 1) };
    }

    auto spec = formTemplate(genre, safeBars);
    std::vector<SectionInfo> sections;
    std::map<SectionKind, int> counts;
    int used = 0;

    for (const auto& item : spec)
    {
        if (used >= safeBars)
            break;
        const int length = juce::jmin(item.second, safeBars - used);
        const int occurrence = ++counts[item.first];
        sections.push_back(makeSection(item.first, used * 4.0, length, occurrence));
        used += length;
    }

    if (used < safeBars && !sections.empty())
        sections.back().lengthBeats += (safeBars - used) * 4.0;

    return sections;
}

namespace
{
    bool partIsEnabled(const MidiGenerationParams& params, Part part)
    {
        if (params.enabledParts == 0)
            return true;
        return isPartLocked(params.enabledParts, part);
    }

    MidiGenerationParams realisedParams(MidiGenerationParams params)
    {
        if (!params.planJson.empty() && params.plannedSections.empty())
        {
            auto plan = ArrangementPlan::parseJson(params.planJson, params);
            plan.applyTo(params);
        }
        return params;
    }
}

GenerationResult ArrangementGenerator::generateArrangement(const MidiGenerationParams& input)
{
    const auto params = realisedParams(input);
    const auto profile = StyleProfile::effectiveGenre(params);
    const unsigned int seed = params.seed != 0 ? params.seed : std::random_device{}();

    GenerationResult result;
    result.sections = !params.plannedSections.empty()
        ? params.plannedSections
        : buildForm(params.genre, params.form, params.lengthInBars);

    if (!params.progression.empty())
    {
        result.progression = params.progression;
    }
    else if (!params.chordText.empty())
    {
        result.progression = theory::parseProgressionText(params.chordText, params.rootNote,
                                                          params.scale, params.lengthInBars,
                                                          profile.defaultExtension);
    }

    if (result.progression.empty())
    {
        auto progressionRng = streamFor(seed, 1);
        result.progression = buildProgression(params, progressionRng);
    }

    const Context ctx{ params, profile, result.progression, result.sections,
                       params.lengthInBars * 4.0, gridSteps(params.quantization) };

    double swing = params.swing > 0.0 ? params.swing : profile.swing;
    bool swingSixteenths = profile.swingSixteenths;
    if (params.motifRhythm == "swung")
    {
        swing = juce::jmax(swing, 0.62);
        swingSixteenths = true;
    }

    std::map<Part, PartNotes> generated;
    PartNotes bassNotes;

    for (const auto part : allParts)
    {
        const bool drumsNeedBass = part == Part::Bass && params.part == Part::Drums;
        if (params.part != Part::All && params.part != part && !drumsNeedBass)
            continue;
        if (!partIsEnabled(params, part) && !drumsNeedBass)
            continue;

        // Independent random stream per part: regenerating one part never changes the others.
        auto rng = streamFor(seed, 10 + static_cast<unsigned int>(part));
        PartNotes notes;

        switch (part)
        {
            case Part::Chords: notes = generateChords(ctx, rng); break;
            case Part::Bass:   notes = generateBass(ctx, rng); break;
            case Part::Synth:  notes = generateSynth(ctx, rng); break;
            case Part::Piano:  notes = generatePiano(ctx, rng); break;
            case Part::Drums:  notes = generateDrums(ctx, bassNotes, rng); break;
            case Part::All:    break;
        }

        if (part != Part::Drums && part != Part::Bass)
        {
            for (int bar = 0; bar + 1 < params.lengthInBars; ++bar)
            {
                const auto* section = ctx.sectionAt(bar * 4.0);
                const auto* nextSection = ctx.sectionAt((bar + 1) * 4.0);
                if (section == nullptr || nextSection == nullptr || section->kind == nextSection->kind)
                    continue;
                if (nextSection->kind != SectionKind::Chorus && nextSection->kind != SectionKind::Drop)
                    continue;
                const double muteFrom = bar * 4.0 + 3.0;
                const double muteTo = (bar + 1) * 4.0;
                notes.erase(std::remove_if(notes.begin(), notes.end(),
                                           [&](const PartNote& n)
                                           {
                                               return n.start >= muteFrom && n.start < muteTo;
                                           }),
                            notes.end());
                for (auto& n : notes)
                    if (n.start < muteFrom && n.start + n.length > muteFrom)
                        n.length = juce::jmax(0.05, muteFrom - n.start);
            }
        }

        if (part == Part::Chords || part == Part::Piano)
        {
            const auto padScale = scalePitches(params.rootNote, params.scale, 48, 88);
            addInnerVoiceMotion(notes, padScale, rng);
        }

        if (part != Part::Drums)
        {
            // Bass stays straight: swing on the low end reads as a broken
            // pocket and pushes the bass swing feature out of the ref box.
            const double partSwing = part == Part::Bass ? 0.5 : swing;
            applySwing(notes, partSwing, swingSixteenths);
            const double humanize = params.humanizeMs > 0.0 ? params.humanizeMs : profile.drums.humanizeMs;
            const double partHuman = part == Part::Drums ? humanize
                                   : part == Part::Bass ? humanize * 0.45
                                   : humanize * 0.8;
            humanizeTiming(notes, rng, partHuman, params.tempo);
            if (part == Part::Chords || part == Part::Piano)
                liftAboveBass(notes, profile.bassHigh);
        }
        finalise(notes, ctx.totalBeats);

        generated[part] = notes;
        if (part == Part::Bass)
            bassNotes = notes;
    }

    if (auto it = generated.find(Part::Bass); it != generated.end())
        snapBassToHarmony(it->second, result.progression, profile.bassLow, profile.bassHigh);
    resolveUnisons(generated, params, result.progression);
    enforceKey(generated, params, result.progression);
    resolveUnisons(generated, params, result.progression);

    for (const auto part : allParts)
    {
        auto it = generated.find(part);
        if (it == generated.end())
            continue;
        if (params.part != Part::All && params.part != part)
            continue;
        if (!partIsEnabled(params, part))
            continue;
        appendToBuffer(it->second, partChannel(part), part == Part::Bass ? 12 : 2, result.midi);
    }

    for (const auto& section : result.sections)
    {
        const int tick = juce::roundToInt(section.startBeat * ticksPerBeat);
        result.midi.addEvent(juce::MidiMessage::textMetaEvent(6, section.label), tick);
    }

    return result;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateChords(const Context& ctx, std::mt19937& rng)
{
    const auto& profile = ctx.profile;

    switch (profile.chordStyle)
    {
        case ChordStyle::Sustained:
        {
            PartNotes notes;
            std::vector<int> previous;
            for (const auto& chord : ctx.progression)
            {
                const double end = chordEnd(chord);
                for (double beat = chord.startBeat; beat < end - 1.0e-6; beat += 1.0)
                {
                    auto voicing = theory::voiceChord(chord, previous, profile.chordVoicing, rng);
                    dropMinorSeconds(voicing);
                    previous = voicing;
                    const double length = juce::jmin(1.0, end - beat) - 0.04;
                    const float energy = ctx.energyAt(beat);
                    for (size_t i = 0; i < voicing.size(); ++i)
                    {
                        const bool top = i + 1 == voicing.size();
                        notes.push_back({ voicing[i], beat, length,
                                          humanise(juce::roundToInt((top ? 80 : 70) * (0.55f + 0.45f * energy)), rng, 4) });
                    }
                }
            }
            return notes;
        }

        case ChordStyle::Arpeggiated:
            return generateArp(ctx, profile.chordArp, juce::jmax(1, ctx.finestSteps), rng, Part::Chords);

        case ChordStyle::Pulse:
        case ChordStyle::Stabs:
        case ChordStyle::Comping:
        default:
            return generatePatternChords(ctx, profile.chordPatterns, profile.chordVoicing,
                                         profile.chordStyle == ChordStyle::Stabs ? 88 : 80, rng,
                                         Part::Chords);
    }
}

ArrangementGenerator::PartNotes ArrangementGenerator::generatePatternChords(
    const Context& ctx, const std::vector<std::string>& patterns,
    const theory::VoicingOptions& voicingOptions, int baseVelocity, std::mt19937& rng,
    Part part)
{
    const auto& progression = ctx.progression;

    std::vector<int> previous;
    std::vector<std::vector<int>> voicingForBar(static_cast<size_t>(juce::jmax(1, ctx.params.lengthInBars)));
    for (int bar = 0; bar < ctx.params.lengthInBars; ++bar)
    {
        const int index = chordIndexAt(progression, bar * 4.0);
        voicingForBar[static_cast<size_t>(bar)] =
            theory::voiceChord(progression[static_cast<size_t>(index)], previous, voicingOptions, rng);
        dropMinorSeconds(voicingForBar[static_cast<size_t>(bar)]);
        previous = voicingForBar[static_cast<size_t>(bar)];
    }

    PartNotes notes;
    const std::string* pattern = &pick(patterns, rng);
    const std::string* fillPattern = patterns.size() > 1 ? &pick(patterns, rng) : pattern;

    std::vector<int> leading = previous;
    for (int bar = 0; bar < ctx.params.lengthInBars; ++bar)
    {
        const std::string* use = (bar % 8 == 7) ? fillPattern : pattern;
        const auto hits = parsePattern(*use);

        bool hasDownbeat = false;
        for (const auto& hit : hits)
            if (hit.step == 0)
                hasDownbeat = true;
        if (!hasDownbeat)
        {
            const auto& bed = voicingForBar[static_cast<size_t>(bar)];
            const float bedEnergy = ctx.energyAt(bar * 4.0);
            for (int pitch : bed)
                notes.push_back({ pitch, bar * 4.0, 0.48,
                                  humanise(juce::roundToInt(58 * (0.45f + 0.55f * bedEnergy)), rng, 3) });
            leading = bed;
        }

        for (size_t hitIndex = 0; hitIndex < hits.size(); ++hitIndex)
        {
            const auto& hit = hits[hitIndex];
            const double start = bar * 4.0 + hit.step * stepBeats;
            const double nextHit = hitIndex + 1 < hits.size()
                ? bar * 4.0 + hits[hitIndex + 1].step * stepBeats
                : (bar + 1) * 4.0;
            double length = juce::jmax(hit.length * stepBeats, nextHit - start);
            int index = chordIndexAt(progression, start);

            bool anticipated = false;
            if (index + 1 < static_cast<int>(progression.size()))
            {
                const double nextStart = progression[static_cast<size_t>(index + 1)].startBeat;
                if (nextStart > start && nextStart - start <= 0.5 + 1.0e-6
                    && uniform(rng) < ctx.profile.anticipation)
                {
                    ++index;
                    anticipated = true;
                }
            }

            if (!anticipated)
                length = juce::jmin(length, chordEnd(progression[static_cast<size_t>(index)]) - start);

            const float density = ctx.densityAt(part, start);
            if (hit.symbol == 'g' && density < 0.45f)
                continue;
            if (hit.symbol != 'X' && density < 0.22f)
                continue;
            if (ctx.energyAt(start) < 0.28f && hit.step % 4 != 0 && hit.symbol != 'X')
                continue;

            const float energy = ctx.energyAt(start);
            const int velocity = juce::roundToInt((hit.symbol == 'X' ? baseVelocity + 10
                               : hit.symbol == 'g' ? baseVelocity - 25
                                                   : baseVelocity) * (0.40f + 0.60f * energy));

            std::vector<int> voicing = theory::voiceChord(progression[static_cast<size_t>(index)],
                                                         leading, voicingOptions, rng);
            dropMinorSeconds(voicing);
            // Cycle inversions by bar so 8-bar loops stay unique without a 2-bar stamp.
            const int spins = (bar + static_cast<int>(hitIndex)) % 3;
            for (int i = 0; i < spins; ++i)
                voicing = invertDiatonic(voicing, voicingOptions);
            dropMinorSeconds(voicing);
            leading = voicing;
            size_t from = 0;
            size_t to = voicing.size();
            if (hit.symbol == 'g' && to > 3)
                from = to - 3;
            if (to - from < 3)
                from = 0;
            if (bar % 8 == 7 && hit.step >= 8 && hit.symbol != 'X')
                continue;
            length = juce::jmin(length, chordEnd(progression[static_cast<size_t>(index)]) - start);
            for (size_t i = from; i < to; ++i)
                notes.push_back({ voicing[i], start, juce::jmax(0.2, length - 0.04),
                                  playedVelocity(velocity, start, rng, 11) });
        }

        // Per-bar diatonic pickup so 4-chord loops still hash uniquely.
        if (!leading.empty() && (bar % 2 == 1 || ctx.profile.genre == Genre::House))
        {
            const int decorate = 9 + (bar % 7);
            const int tone = leading[static_cast<size_t>(bar) % leading.size()];
            if (pitchAllowed(tone, progression[static_cast<size_t>(chordIndexAt(progression, bar * 4.0))],
                             ctx.params.rootNote, ctx.params.scale))
                notes.push_back({ tone, bar * 4.0 + decorate * stepBeats, 0.16,
                                  playedVelocity(64, bar * 4.0 + decorate * stepBeats, rng, 8) });
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateBass(const Context& ctx, std::mt19937& rng)
{
    const bool walking = ctx.profile.bassStyle == BassStyle::Walking;
    const bool useTwoFeel = walking && !ctx.profile.bassPatterns.empty() && uniform(rng) < 0.72;
    auto notes = (walking && !useTwoFeel) ? generateWalkingBass(ctx, rng)
                                          : generatePatternBass(ctx, rng);
    addSlides(notes, ctx.profile.slideChance, rng);
    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generatePatternBass(const Context& ctx, std::mt19937& rng)
{
    const auto& profile = ctx.profile;
    const auto& progression = ctx.progression;
    const int low = profile.bassLow;
    const int high = profile.bassHigh;

    std::vector<std::string> approachPatterns;
    for (const auto& pattern : profile.bassPatterns)
        if (pattern.find('A') != std::string::npos)
            approachPatterns.push_back(pattern);

    PartNotes notes;
    int previousRoot = foldIntoRange(theory::nearestPitchWithClass(progression.front().rootPitchClass,
                                                                   low + 7), low, high);
    const std::string* primary = &pick(profile.bassPatterns, rng);
    const std::string* pattern = primary;

    for (int bar = 0; bar < ctx.params.lengthInBars; ++bar)
    {
        if (bar % 8 == 7 && !approachPatterns.empty())
            pattern = &pick(approachPatterns, rng);
        else
            pattern = primary;

        for (const auto& hit : parsePattern(*pattern))
        {
            const double start = bar * 4.0 + hit.step * stepBeats;
            const int index = chordIndexAt(progression, start);
            const auto& chord = progression[static_cast<size_t>(index)];
            const auto& next = progression[static_cast<size_t>(index + 1) % progression.size()];

            const int root = foldIntoRange(theory::nearestPitchWithClass(chord.rootPitchClass, previousRoot),
                                           low, high);
            const char role = static_cast<char>(std::toupper(static_cast<unsigned char>(hit.symbol)));
            int pitch = root;

            switch (role)
            {
                case 'F': pitch = root + 7 <= high ? root + 7 : root - 5; break;
                case 'O': pitch = root + 12 <= high ? root + 12 : root; break;
                case 'T': pitch = root + (chord.intervals.size() > 1 ? chord.intervals[1] % 12 : 4); break;
                case 'A':
                {
                    const int nextRoot = foldIntoRange(theory::nearestPitchWithClass(next.rootPitchClass, root),
                                                       low, high);
                    if (nextRoot == root)
                        pitch = root + 7 <= high ? root + 7 : root - 5;
                    else if (ctx.profile.genre == Genre::RnB)
                    {
                        const int direction = nextRoot >= root ? 1 : -1;
                        int candidate = nextRoot - direction;
                        if (!pitchAllowed(candidate, chord, ctx.params.rootNote, ctx.params.scale))
                        {
                            const int fifth = root + 7 <= high ? root + 7 : root - 5;
                            const int third = root + (chord.intervals.size() > 1 ? chord.intervals[1] % 12 : 4);
                            candidate = std::abs(fifth - nextRoot) <= std::abs(third - nextRoot) ? fifth : third;
                        }
                        pitch = candidate;
                    }
                    else
                    {
                        const int fifth = root + 7 <= high ? root + 7 : root - 5;
                        const int third = root + (chord.intervals.size() > 1 ? chord.intervals[1] % 12 : 4);
                        pitch = std::abs(fifth - nextRoot) <= std::abs(third - nextRoot) ? fifth : third;
                    }
                    break;
                }
                default: break;
            }

            pitch = foldIntoRange(pitch, low - 2, high + 2);

            // Strong beats (1 and 3) stay on the root or fifth so the bass agrees with the harmony.
            if (hit.step % 8 == 0)
            {
                const int fifth = root + 7 <= high ? root + 7 : root - 5;
                pitch = (hit.step == 0 || role == 'R') ? root : fifth;
            }
            else if (hit.step % 4 == 0 && role != 'A')
            {
                const int fifth = root + 7 <= high ? root + 7 : root - 5;
                if (pitch % 12 != root % 12 && pitch % 12 != fifth % 12)
                    pitch = fifth;
            }

            double length = hit.length * stepBeats;
            length = juce::jmin(length, chordEnd(chord) - start);

            const bool ghost = std::islower(static_cast<unsigned char>(hit.symbol)) != 0;
            const float density = ctx.densityAt(Part::Bass, start);
            if (ghost && density < 0.45f)
                continue;
            if (role != 'R' && hit.step != 0 && density < 0.28f)
                continue;
            const int velocity = ghost ? 52
                : hit.step == 0 ? (ctx.profile.genre == Genre::RnB ? 110 : 104)
                                : (ctx.profile.genre == Genre::RnB ? 74 : 88);
            notes.push_back({ pitch, start, length - 0.02, playedVelocity(velocity, start, rng, 16) });
            previousRoot = root;
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateWalkingBass(const Context& ctx, std::mt19937& rng)
{
    const auto& profile = ctx.profile;
    const auto& progression = ctx.progression;
    const int low = profile.bassLow;
    const int high = profile.bassHigh;
    const auto keyScale = scalePitches(ctx.params.rootNote, ctx.params.scale, low, high);

    PartNotes notes;
    int current = foldIntoRange(theory::nearestPitchWithClass(progression.front().rootPitchClass, low + 10),
                                low, high);

    for (size_t index = 0; index < progression.size(); ++index)
    {
        const auto& chord = progression[index];
        const auto scale = chordScale(keyScale, chord, ctx.params.rootNote, ctx.params.scale);
        const auto& next = progression[(index + 1) % progression.size()];
        const int beats = juce::jmax(1, static_cast<int>(std::round(chord.lengthBeats)));

        const int root = foldIntoRange(theory::nearestPitchWithClass(chord.rootPitchClass, current), low, high);
        const int nextRoot = foldIntoRange(theory::nearestPitchWithClass(next.rootPitchClass, root), low, high);
        const auto chordTones = chordTonePitches(chord, true, low, high);

        current = root;
        notes.push_back({ root, chord.startBeat, 0.92, playedVelocity(96, chord.startBeat, rng, 14) });

        for (int beat = 1; beat < beats; ++beat)
        {
            if (beat % 2 == 1 && uniform(rng) < 0.48)
                continue;
            int pitch;
            if (beat == beats - 1)
            {
                int chordApproach = 0;
                bool found = false;
                for (int tone : chordTones)
                {
                    if (std::abs(tone - nextRoot) <= 3 && tone != nextRoot)
                    {
                        chordApproach = tone;
                        found = true;
                        if (std::abs(tone - nextRoot) == 1)
                            break;
                    }
                }
                if (found)
                    pitch = foldIntoRange(chordApproach, low, high);
                else if (nextRoot == current)
                    pitch = scaleMove(scale, current, uniform(rng) < 0.5 ? 1 : -1);
                else
                    pitch = foldIntoRange(root + 7 <= high ? root + 7 : root - 5, low, high);
            }
            else if (beat % 2 == 0)
            {
                pitch = foldIntoRange(root + 7 <= high ? root + 7 : root - 5, low, high);
            }
            else
            {
                const int remaining = beats - beat;
                const double ideal = current + (nextRoot - current) / static_cast<double>(remaining + 1);
                std::vector<int> candidates = scale;
                candidates.insert(candidates.end(), chordTones.begin(), chordTones.end());

                double bestScore = 1.0e9;
                pitch = current;
                for (int candidate : candidates)
                {
                    if (candidate == current || std::abs(candidate - current) > 7)
                        continue;
                    const double score = std::abs(candidate - ideal) + 2.0 * uniform(rng)
                                       + (theory::containsPitchClass(chord, candidate) ? 0.0 : 1.8);
                    if (score < bestScore)
                    {
                        bestScore = score;
                        pitch = candidate;
                    }
                }
            }

            pitch = foldIntoRange(pitch, low, high);
            notes.push_back({ pitch, chord.startBeat + beat, 0.92,
                              playedVelocity(beat % 2 == 1 ? 82 : 90, chord.startBeat + beat, rng, 16) });
            current = pitch;
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateSynth(const Context& ctx, std::mt19937& rng)
{
    if (ctx.profile.leadStyle == LeadStyle::Arp)
        return generateArp(ctx, ctx.profile.leadArp, ctx.finestSteps, rng, Part::Synth);

    return generateMelody(ctx, ctx.profile.leadMelody, rng, Part::Synth);
}

ArrangementGenerator::PartNotes ArrangementGenerator::generatePiano(const Context& ctx, std::mt19937& rng)
{
    const auto& profile = ctx.profile;

    switch (profile.pianoStyle)
    {
        case PianoStyle::Broken:   return generateBrokenPiano(ctx, rng);
        case PianoStyle::Comp:     return generatePatternChords(ctx, profile.pianoPatterns, profile.pianoVoicing, 82, rng, Part::Piano);
        case PianoStyle::Melodic:  return generateMelody(ctx, profile.pianoMelody, rng, Part::Piano);
        case PianoStyle::Arpeggio: return generateArp(ctx, profile.pianoArp, ctx.finestSteps, rng, Part::Piano);
    }

    return {};
}

namespace gm
{
    constexpr int kick = 36;
    constexpr int rim = 37;
    constexpr int snare = 38;
    constexpr int clap = 39;
    constexpr int eSnare = 40;
    constexpr int floorTom = 41;
    constexpr int closedHat = 42;
    constexpr int pedalHat = 44;
    constexpr int lowTom = 45;
    constexpr int openHat = 46;
    constexpr int midTom = 47;
    constexpr int crash = 49;
    constexpr int hiTom = 50;
    constexpr int ride = 51;
    constexpr int tamb = 54;
    constexpr int maracas = 70;
}

double drumLength(int pitch)
{
    switch (pitch)
    {
        case gm::kick:      return 0.45;
        case gm::snare:
        case gm::eSnare:
        case gm::clap:      return 0.22;
        case gm::closedHat:
        case gm::pedalHat:  return 0.10;
        case gm::openHat:   return 0.70;
        case gm::crash:     return 2.2;
        case gm::ride:      return 0.85;
        case gm::rim:       return 0.12;
        case gm::maracas:
        case gm::tamb:      return 0.14;
        default:            return 0.32;
    }
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateDrums(const Context& ctx,
                                                                    const PartNotes& bassNotes,
                                                                    std::mt19937& rng)
{
    const auto& drums = ctx.profile.drums;
    const double tempo = ctx.params.tempo;
    const auto msToBeats = [tempo](double ms) { return ms * tempo / 60000.0; };

    auto swingWarp = [](double beat, double unit, double swing)
    {
        if (swing <= 0.5 + 1.0e-6)
            return beat;
        const double base = std::floor(beat / unit + 1.0e-9) * unit;
        const double fraction = (beat - base) / unit;
        const double warped = fraction < 0.5 ? fraction * (swing / 0.5)
                                             : swing + (fraction - 0.5) * ((1.0 - swing) / 0.5);
        return base + warped * unit;
    };

    PartNotes notes;
    if (drums.grooves.empty())
        return notes;

    auto chokeOpen = [&](double start)
    {
        for (auto& note : notes)
            if (note.pitch == gm::openHat && note.start < start
                && note.start + note.length > start)
                note.length = juce::jmax(0.03, start - note.start);
    };

    auto addHit = [&](int pitch, double beat, int velocity, bool isHat, int bend = 8192,
                      double length = -1.0)
    {
        double t = beat;
        if (isHat && drums.hatSwing > 0.5)
            t = swingWarp(t, 0.25, drums.hatSwing);

        t += msToBeats(drums.laidBackMs);
        if (isHat)
            t += msToBeats(drums.hatLagMs);
        const double humanize = ctx.params.humanizeMs > 0.0 ? ctx.params.humanizeMs : drums.humanizeMs;
        t += (uniform(rng) * 2.0 - 1.0) * msToBeats(humanize);
        t = juce::jmax(0.0, t);

        if (pitch == gm::closedHat || pitch == gm::pedalHat)
            chokeOpen(t);

        notes.push_back({ pitch, t, length > 0.0 ? length : drumLength(pitch),
                          playedVelocity(velocity, t, rng, isHat ? 14 : 10), false, bend });
    };

    auto emitLane = [&](const std::string& pattern, int pitch, int accentVel,
                        int hitVel, int ghostVel, bool isHat, int bar, float energy)
    {
        if (pattern.empty())
            return;
        const int size = static_cast<int>(pattern.size());
        for (int step = 0; step < 16; ++step)
        {
            const char c = pattern[static_cast<size_t>(step % size)];
            if (c == '.' || c == '-')
                continue;
            int usePitch = pitch;
            if (isHat && pitch == gm::closedHat && (c == 'g' || c == 'p'))
                usePitch = gm::pedalHat;
            const int vel = juce::roundToInt((c == 'X' ? accentVel : c == 'g' ? ghostVel : hitVel)
                                             * (0.82f + 0.18f * energy));
            addHit(usePitch, bar * 4.0 + step * stepBeats, vel, isHat);
        }
    };

    const int bars = ctx.params.lengthInBars;
    const int nGrooves = juce::jmax(1, static_cast<int>(drums.grooves.size()));
    int grooveIndex = ctx.params.grooveIndex;
    if (grooveIndex < 0 || grooveIndex >= nGrooves)
        grooveIndex = static_cast<int>(rng() % static_cast<unsigned>(nGrooves));

    for (int bar = 0; bar < bars; ++bar)
    {
        if (bar > 0 && bar % 8 == 0)
            grooveIndex = (grooveIndex + 1) % nGrooves;

        const auto& groove = drums.grooves[static_cast<size_t>(grooveIndex)];
        const float energy = ctx.energyAt(bar * 4.0);
        const auto* section = ctx.sectionAt(bar * 4.0);
        const auto* nextSection = bar + 1 < bars ? ctx.sectionAt((bar + 1) * 4.0) : nullptr;
        const bool sectionStart = bar == 0
            || (section != nullptr && std::abs(section->startBeat - bar * 4.0) < 1.0e-6);
        const bool liftComing = section != nullptr && nextSection != nullptr
                             && nextSection->kind != section->kind
                             && (nextSection->kind == SectionKind::Chorus
                                 || nextSection->kind == SectionKind::Drop);
        const bool fillBar = liftComing || (bar % 4 == 3);

        if (!drums.lockKickToBass)
            emitLane(groove.kick, gm::kick, drums.kickVelocity, drums.kickVelocity - 8,
                     drums.kickVelocity - 20, false, bar, energy);
        emitLane(groove.snare, gm::snare, drums.snareVelocity, drums.snareVelocity - 6,
                 juce::jmax(28, drums.snareVelocity - 28), false, bar, energy);
        emitLane(groove.clap, gm::clap, drums.snareVelocity - 6, drums.snareVelocity - 12,
                 drums.snareVelocity - 24, false, bar, energy);

        const int hatPitch = drums.rideHats ? gm::ride : gm::closedHat;
        emitLane(groove.closedHat, hatPitch, drums.hatVelocity + 4, drums.hatVelocity,
                 drums.hatVelocity - 10, true, bar, energy);
        emitLane(groove.openHat, gm::openHat, drums.hatVelocity + 2, drums.hatVelocity - 2,
                 drums.hatVelocity - 12, true, bar, energy);
        emitLane(groove.tom, gm::lowTom, drums.kickVelocity - 8, drums.kickVelocity - 16,
                 drums.kickVelocity - 24, false, bar, energy);

        if (ctx.profile.genre == Genre::Cinematic && sectionStart)
            addHit(gm::crash, bar * 4.0, 96, false);

        if (!fillBar)
            continue;

        const double fillStart = bar * 4.0 + 3.0;
        const double fillEnd = (bar + 1) * 4.0;

        switch (ctx.profile.genre)
        {
            case Genre::House:
                addHit(gm::clap, fillStart, 78, false);
                addHit(gm::clap, fillStart + 0.5, 86, false);
                addHit(gm::kick, fillEnd - 0.25, drums.kickVelocity, false);
                break;
            case Genre::Trap:
            {
                // Occasional 32nd hat roll on the last beat only.
                const int count = 8;
                const double dt = 1.0 / count;
                for (int i = 0; i < count; ++i)
                {
                    const int vel = 32 + i * 3;
                    const int bend = 8192 + i * 180;
                    addHit(gm::closedHat, fillStart + i * dt, vel, true, bend, dt * 0.8);
                }
                break;
            }
            case Genre::LoFiJazz:
                addHit(gm::lowTom, fillStart, 82, false);
                addHit(gm::snare, fillEnd - 0.25, drums.snareVelocity - 4, false);
                break;
            case Genre::Pop:
                addHit(gm::snare, fillStart + 0.5, 88, false);
                addHit(gm::snare, fillEnd - 0.25, drums.snareVelocity - 6, false);
                break;
            case Genre::RnB:
                addHit(gm::snare, fillStart + 0.5, 70, false);
                addHit(gm::snare, fillEnd - 0.25, 84, false);
                break;
            case Genre::Cinematic:
                addHit(gm::floorTom, fillStart, 100, false);
                addHit(gm::lowTom, fillStart + 0.5, 104, false);
                break;
        }

        if (liftComing && bar + 1 < bars)
            addHit(gm::crash, (bar + 1) * 4.0, 100, false);
    }

    // One-beat dropout into a chorus/drop so the lift reads louder.
    for (int bar = 0; bar + 1 < bars; ++bar)
    {
        const auto* section = ctx.sectionAt(bar * 4.0);
        const auto* nextSection = ctx.sectionAt((bar + 1) * 4.0);
        if (section == nullptr || nextSection == nullptr || section->kind == nextSection->kind)
            continue;
        if (nextSection->kind != SectionKind::Chorus && nextSection->kind != SectionKind::Drop)
            continue;
        const double muteFrom = bar * 4.0 + 3.0;
        const double muteTo = (bar + 1) * 4.0 - 0.02;
        notes.erase(std::remove_if(notes.begin(), notes.end(),
                                   [&](const PartNote& n)
                                   {
                                       if (n.start < muteFrom || n.start >= muteTo)
                                           return false;
                                       return n.pitch == gm::kick || n.pitch == gm::snare
                                           || n.pitch == gm::clap || n.pitch == gm::eSnare;
                                   }),
                    notes.end());
    }

    // Trap: sparse kick locked to the 808, plus a downbeat if the bar is empty.
    if (drums.lockKickToBass)
    {
        for (const auto& bass : bassNotes)
        {
            if (bass.velocity < 88)
                continue;
            bool nearby = false;
            for (const auto& n : notes)
            {
                if (n.pitch == gm::kick && std::abs(n.start - bass.start) < 0.10)
                {
                    nearby = true;
                    break;
                }
            }
            if (!nearby)
                addHit(gm::kick, bass.start, juce::jmin(118, bass.velocity + 4), false);
        }
        for (int bar = 0; bar < bars; ++bar)
        {
            const double t = bar * 4.0;
            bool hasKick = false;
            for (const auto& n : notes)
                if (n.pitch == gm::kick && n.start >= t - 0.02 && n.start < t + 1.0)
                    hasKick = true;
            if (!hasKick)
                addHit(gm::kick, t, drums.kickVelocity, false);
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateBrokenPiano(const Context& ctx, std::mt19937& rng)
{
    PartNotes notes;
    std::vector<int> previous;

    for (const auto& chord : ctx.progression)
    {
        auto voicing = theory::voiceChord(chord, previous, ctx.profile.pianoVoicing, rng);
        dropMinorSeconds(voicing);
        previous = voicing;
        if (voicing.empty())
            continue;

        const auto v = [&voicing](size_t i) { return voicing[juce::jmin(i, voicing.size() - 1)]; };
        const int sequence[] = { v(0), v(1), v(2), v(1), v(0), v(2), v(1), v(0) };

        const double end = chordEnd(chord);
        const bool trapPiano = ctx.profile.genre == Genre::Trap;
        const double rate = trapPiano ? 0.25 : 0.5;
        const double gate = trapPiano ? 0.22 : juce::jmin(1.1, rate + 0.18);
        int index = 0;
        for (double beat = chord.startBeat; beat < end - 1.0e-6; beat += rate, ++index)
        {
            const int bar = static_cast<int>(std::floor(beat / 4.0 + 1.0e-9));
            const int rotate = bar % 8;
            if (trapPiano && index % 7 == (bar % 7))
                continue;
            const int velocity = index % 8 == 0 ? 96 : index % 2 == 0 ? 74 : 50;
            const int pitch = sequence[(index + rotate) % 8];
            const double noteLen = juce::jmin(gate, end - beat);
            notes.push_back({ pitch, beat, noteLen, playedVelocity(velocity, beat, rng, trapPiano ? 14 : 12) });
        }
        if (trapPiano && !voicing.empty())
        {
            const int bar = static_cast<int>(std::floor(chord.startBeat / 4.0 + 1.0e-9));
            const int pickup = voicing[(bar + 1) % static_cast<int>(voicing.size())];
            notes.push_back({ pickup, chord.startBeat + 3.0 + (bar % 4) * 0.25, 0.16,
                              humanise(66, rng, 10) });
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateArp(const Context& ctx, const ArpProfile& role,
                                                                  int rateSteps, std::mt19937& rng, Part part)
{
    PartNotes notes;
    const auto& shape = pick(role.shapes, rng);
    const double rate = juce::jmax(1, rateSteps) * stepBeats;
    std::vector<int> previous;

    theory::VoicingOptions options;
    options.low = role.low;
    options.high = juce::jmin(role.high, role.low + 19);
    options.maxNotes = 4;

    for (const auto& chord : ctx.progression)
    {
        auto base = theory::voiceChord(chord, previous, options, rng);
        dropMinorSeconds(base);
        previous = base;

        std::vector<int> tones = base;
        for (int note : base)
            if (note + 12 <= role.high && tones.size() < 8)
                tones.push_back(note + 12);
        std::sort(tones.begin(), tones.end());

        const double end = chordEnd(chord);
        size_t index = 0;
        for (double beat = chord.startBeat; beat < end - 1.0e-6; beat += rate, ++index)
        {
            const float density = ctx.densityAt(part, beat);
            if (index % 2 == 1 && uniform(rng) > 0.35 + 0.65 * density)
                continue;
            const int rotate = static_cast<int>(std::floor(chord.startBeat / 4.0 + 1.0e-9));
            const int pitch = tones[static_cast<size_t>(shape[(index + rotate) % shape.size()]) % tones.size()];
            const double inBeat = beat - std::floor(beat);
            const int velocity = role.velocity + (inBeat < 1.0e-6 ? 10 : std::abs(inBeat - 0.5) < 1.0e-6 ? 3 : -4);
            notes.push_back({ pitch, beat, rate * role.gate, humanise(velocity, rng, 12) });
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateMelody(const Context& ctx, const MelodyProfile& roleIn,
                                                                     std::mt19937& rng, Part part)
{
    struct RhythmNote
    {
        int step;
        int length;
    };
    using Rhythm = std::vector<RhythmNote>;

    MelodyProfile role = roleIn;
    if (ctx.params.motifRhythm == "sparse")
    {
        role.density *= 0.55;
        role.restChance = juce::jmax(role.restChance, 0.5);
    }
    else if (ctx.params.motifRhythm == "syncopated")
    {
        role.syncopation = juce::jmax(role.syncopation, 0.55);
        role.density = juce::jmin(1.0, role.density + 0.1);
    }

    const auto& progression = ctx.progression;
    const int keyRoot = ctx.params.rootNote;
    const int bars = ctx.params.lengthInBars;
    const int finest = (role.syncopation >= 0.32
                        || ctx.profile.genre == Genre::Pop
                        || ctx.profile.genre == Genre::House
                        || ctx.profile.genre == Genre::Trap)
        ? 1
        : juce::jmax(2, ctx.finestSteps);
    const int motifBars = juce::jlimit(1, juce::jmax(1, bars), role.motifBars);
    const int motifSteps = motifBars * 16;
    const auto keyScale = scalePitches(keyRoot, ctx.params.scale, role.low, role.high);

    auto makeRhythm = [&]() -> Rhythm
    {
        std::vector<int> onsets;
        for (int step = 0; step < motifSteps; step += finest)
        {
            const int inBar = step % 16;
            const double weight = inBar == 0 ? 1.0 : inBar % 8 == 0 ? 0.8 : inBar % 4 == 0 ? 0.55
                                : inBar % 2 == 0 ? 0.3 : 0.15;
            const double offbeat = inBar % 4 == 2 ? 0.35 : inBar % 2 == 1 ? 0.22 : 0.0;
            const double chance = step == 0 ? 0.92 : role.density * (0.45 + weight) + role.syncopation * offbeat;
            if (uniform(rng) < chance)
                onsets.push_back(step);
        }

        if (uniform(rng) < role.restChance)
            onsets.erase(std::remove_if(onsets.begin(), onsets.end(),
                                        [&](int s) { return s > 0 && s >= motifSteps - 4; }),
                         onsets.end());

        if (onsets.size() < 2)
            onsets = { 0, motifSteps / 2 };

        Rhythm rhythm;
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const int next = i + 1 < onsets.size() ? onsets[i + 1] : motifSteps;
            int length = next - onsets[i];
            const int maxLength = uniform(rng) < role.longNoteChance ? 16 : 6;
            length = juce::jmin(length, maxLength);
            if (length >= 2 * finest && uniform(rng) < role.restChance * 0.4)
                length = juce::jmax(finest, (length / 2 / finest) * finest);
            rhythm.push_back({ onsets[i], juce::jmax(1, length) });
        }
        return rhythm;
    };

    auto mutateRhythm = [&](Rhythm rhythm) -> Rhythm
    {
        if (rhythm.size() > 2 && uniform(rng) < 0.5)
        {
            const size_t drop = 1 + rng() % (rhythm.size() - 1);
            rhythm[drop - 1].length += rhythm[drop].length;
            rhythm.erase(rhythm.begin() + static_cast<long>(drop));
        }
        for (size_t i = 0; i < rhythm.size(); ++i)
        {
            if (rhythm[i].length >= 4 * finest && uniform(rng) < 0.5)
            {
                const int half = (rhythm[i].length / 2 / finest) * finest;
                rhythm.insert(rhythm.begin() + static_cast<long>(i) + 1,
                              { rhythm[i].step + half, rhythm[i].length - half });
                rhythm[i].length = half;
                break;
            }
        }
        return rhythm;
    };

    auto makeSyncPluck = [&](int nOdd, int nEighth, int nBeat) -> Rhythm
    {
        // Mix odd-16ths / eighth-offs / beats in a 2:1:1 ratio so syncopation
        // lands at 0.625, the piano-performance box leads are scored against.
        const int eighths[4] = { 2, 6, 10, 14 };
        const int beats[4] = { 0, 4, 8, 12 };
        std::vector<int> steps;
        auto take = [&](const int* pool, int poolN, int count)
        {
            std::vector<int> available(pool, pool + poolN);
            const int n = juce::jmin(count, poolN);
            for (int i = 0; i < n; ++i)
            {
                const int pick = static_cast<int>(rng() % available.size());
                steps.push_back(available[static_cast<size_t>(pick)]);
                available.erase(available.begin() + pick);
            }
        };
        take(beats, 4, nBeat);
        take(eighths, 4, nEighth);
        std::vector<int> odds{ 1, 3, 5, 7, 9, 11, 13, 15 };
        take(odds.data(), 8, nOdd);
        std::sort(steps.begin(), steps.end());
        steps.erase(std::unique(steps.begin(), steps.end()), steps.end());
        Rhythm rhythm;
        for (size_t i = 0; i < steps.size(); ++i)
        {
            const int next = i + 1 < steps.size() ? steps[i + 1] : 16;
            rhythm.push_back({ steps[i], juce::jmax(1, next - steps[i]) });
        }
        return rhythm;
    };

    auto makeContour = [&](size_t count) -> std::vector<int>
    {
        std::vector<int> deltas{ 0 };
        for (size_t i = 1; i < count; ++i)
        {
            const int previous = deltas.back();
            const auto contour = ctx.params.motifContour;
            double upChance = i < count / 2 ? 0.62 : 0.38;
            if (contour == "rising")
                upChance = 0.82;
            else if (contour == "falling")
                upChance = 0.18;
            else if (contour == "static")
                upChance = 0.5;
            const int direction = uniform(rng) < upChance ? 1 : -1;
            int delta;

            double intervalSum = 0.0;
            for (double w : role.intervalWeights)
                intervalSum += w;

            if (intervalSum > 1.0e-6 && std::abs(previous) < 3)
            {
                double cursor = uniform(rng) * intervalSum;
                delta = 0;
                for (int k = 0; k < 13; ++k)
                {
                    cursor -= role.intervalWeights[static_cast<size_t>(k)];
                    if (cursor <= 0.0)
                    {
                        delta = k - 6;
                        break;
                    }
                }
            }
            else if (std::abs(previous) >= 3)
                delta = previous > 0 ? -1 : 1;     // fill the gap after a leap
            else
            {
                const double r = uniform(rng);
                const double stay = contour == "static" ? 0.55 : 0.25;
                delta = r < 0.12 && contour != "static" ? direction * (3 + static_cast<int>(rng() % 2))
                      : r < stay ? 0
                      : r < stay + 0.4 ? direction
                                       : direction * (contour == "static" ? 1 : 2);
            }
            deltas.push_back(delta);
        }
        return deltas;
    };

    auto varyEnding = [&](std::vector<int> contour)
    {
        for (size_t i = contour.size() > 2 ? contour.size() - 2 : 1; i < contour.size(); ++i)
            contour[i] = (uniform(rng) < 0.5 ? -1 : 1) * (1 + static_cast<int>(rng() % 2));
        return contour;
    };

    const bool syncPluck = part == Part::Synth && ctx.profile.genre == Genre::Trap;
    const Rhythm rhythmA = syncPluck ? makeSyncPluck(4, 2, 2) : makeRhythm();
    auto contourA = makeContour(rhythmA.size());
    if (contourA.size() >= 4)
    {
        // Memorable 4-note hook: neighbour / skip, then the inversion answers.
        contourA[0] = 0;
        contourA[1] = uniform(rng) < 0.55 ? 2 : 1;
        contourA[2] = uniform(rng) < 0.6 ? contourA[1] : 0;
        contourA[3] = -contourA[1];
    }
    const auto contourA2 = varyEnding(contourA);
    const Rhythm rhythmB = syncPluck ? makeSyncPluck(5, 2, 1) : mutateRhythm(rhythmA);
    auto contourB = makeContour(rhythmB.size());
    if (contourB.size() >= 4)
    {
        for (size_t i = 0; i < 4 && i < contourB.size(); ++i)
            contourB[i] = -contourA[juce::jmin(i, contourA.size() - 1)];
    }
    const bool contrastInSecondPhrase = true;

    const int numSegments = (bars + motifBars - 1) / motifBars;
    const int segmentsPerPhrase = juce::jmax(1, 4 / motifBars);

    const int centre = (role.low + role.high) / 2 + static_cast<int>(rng() % 7) - 3;
    int current = nearestOf(chordTonePitches(progression.front(), false, role.low, role.high), centre, centre);
    int lastPitch = -1;
    int repeats = 0;

    PartNotes notes;

    for (int segment = 0; segment < numSegments; ++segment)
    {
        const int position = segment % segmentsPerPhrase;
        const bool lastSegment = segment == numSegments - 1;
        const bool phraseEnd = position == segmentsPerPhrase - 1 || lastSegment;
        const bool secondPhrase = segment >= segmentsPerPhrase;

        const Rhythm* rhythm = &rhythmA;
        const std::vector<int>* contour = &contourA;

        if (segmentsPerPhrase >= 4)
        {
            if (position == 2)              contour = &contourA2;
            else if (position == 3)         { rhythm = &rhythmB; contour = &contourB; }
        }
        else if (position == 1)
        {
            contour = &contourA2;
        }
        else if (secondPhrase && contrastInSecondPhrase)
        {
            rhythm = &rhythmB;
            contour = &contourB;
        }

        Rhythm segmentRhythm = *rhythm;
        if (phraseEnd && !syncPluck)
        {
            // Cadence: stop by beat 3 of the phrase's last bar and hold the final note.
            const int lastOnset = motifSteps - 8;
            segmentRhythm.erase(std::remove_if(segmentRhythm.begin(), segmentRhythm.end(),
                                               [&](const RhythmNote& n) { return n.step > lastOnset && n.step != 0; }),
                                segmentRhythm.end());
            if (segmentRhythm.empty())
                segmentRhythm.push_back({ 0, motifSteps - 2 });
            auto& final = segmentRhythm.back();
            final.length = juce::jmax(final.length, motifSteps - final.step - (lastSegment ? 0 : 2));
        }

        const int segmentStep = segment * motifSteps;

        for (size_t k = 0; k < segmentRhythm.size(); ++k)
        {
            const auto& rn = segmentRhythm[k];
            const double start = (segmentStep + rn.step) * stepBeats;
            if (start >= ctx.totalBeats - 1.0e-6)
                break;

            const auto& chord = progression[static_cast<size_t>(chordIndexAt(progression, start))];
            const bool strong = rn.step % 8 == 0 || rn.length >= 4;
            const bool cadenceNote = phraseEnd && k + 1 == segmentRhythm.size();
            const int delta = k < contour->size() ? (*contour)[k] : 0;
            const auto scale = chordScale(keyScale, chord, keyRoot, ctx.params.scale);

            int proposal = scaleMove(scale, current, delta);
            if (proposal >= role.high - 1 || proposal <= role.low + 1)
                proposal = scaleMove(scale, current, -delta);

            int pitch = proposal;

            if (cadenceNote)
            {
                auto tones = chordTonePitches(chord, false, role.low, role.high);
                if (lastSegment && theory::containsPitchClass(chord, keyRoot))
                {
                    std::vector<int> tonics;
                    for (int tone : tones)
                        if (tone % 12 == keyRoot)
                            tonics.push_back(tone);
                    if (!tonics.empty())
                        tones = tonics;
                }
                pitch = nearestOf(tones, proposal, proposal);
            }
            else if (strong)
            {
                pitch = nearestOf(chordTonePitches(chord, role.useExtensions, role.low, role.high), proposal, proposal);
            }
            else if (rn.length >= 3 && !theory::containsPitchClass(chord, pitch))
            {
                const int nearestTone = nearestOf(chordTonePitches(chord, role.useExtensions, role.low, role.high),
                                                  pitch, pitch);
                if (std::abs(nearestTone - pitch) == 1)
                    pitch = nearestTone;
            }

            if (pitch == lastPitch)
            {
                if (++repeats >= 2)
                {
                    pitch = scaleMove(scale, pitch, uniform(rng) < 0.5 ? 1 : -1);
                    repeats = 0;
                }
            }
            else
            {
                repeats = 0;
            }

            if (lastPitch >= 0 && std::abs(pitch - lastPitch) > 9)
            {
                const int shifted = pitch + (pitch > lastPitch ? -12 : 12);
                if (shifted >= role.low && shifted <= role.high)
                    pitch = shifted;
            }

            const float energy = ctx.energyAt(start);
            const float density = ctx.densityAt(part, start);
            const bool trapLead = ctx.profile.genre == Genre::Trap && part == Part::Synth;
            if (!syncPluck && !strong && !cadenceNote && uniform(rng) > 0.50 + 0.50 * energy * density)
                continue;
            const double shortSkip = syncPluck ? 0.08 : (ctx.profile.genre == Genre::Trap ? 0.22 : 0.58);
            if (!cadenceNote && k > 0 && rn.length <= 2 * finest
                && segmentRhythm[k - 1].length <= 2 * finest && uniform(rng) < shortSkip)
                continue;

            const double beatInTwoBars = std::fmod(start, 8.0);
            if (!cadenceNote && !syncPluck)
            {
                if (part == Part::Piano && beatInTwoBars < 4.0 && uniform(rng) < 0.38)
                    continue;
                if (part == Part::Synth && beatInTwoBars >= 4.0 && !strong && uniform(rng) < 0.32)
                    continue;
            }

            pitch = foldIntoRange(pitch, role.low, role.high);
            // Piano-performance refs live around 66-70; keep the hook there.
            while (pitch > 71 && pitch - 12 >= role.low)
                pitch -= 12;
            while (pitch < 60 && pitch + 12 <= role.high)
                pitch += 12;

            const int velocity = juce::roundToInt((role.velocity + (strong ? 6 : -4) - (cadenceNote ? 4 : 0))
                                                  * (0.55f + 0.45f * energy));
            const double length = rn.length * stepBeats * (cadenceNote ? 1.0 : 0.92);
            notes.push_back({ pitch, start, length, playedVelocity(velocity, start, rng, syncPluck ? 16 : 13) });

            if (role.dyads && strong && uniform(rng) < 0.18)
            {
                std::vector<int> below;
                for (int tone : chordTonePitches(chord, role.useExtensions, pitch - 9, pitch - 3))
                    below.push_back(tone);
                if (!below.empty())
                    notes.push_back({ nearestOf(below, pitch - 4, pitch - 4), start, length,
                                      humanise(velocity - 14, rng, 4) });
            }

            current = pitch;
            lastPitch = pitch;
        }
    }

    return notes;
}

void ArrangementGenerator::addSlides(PartNotes& notes, double chance, std::mt19937& rng)
{
    if (chance <= 0.0)
        return;

    std::sort(notes.begin(), notes.end(), [](const PartNote& a, const PartNote& b) { return a.start < b.start; });

    for (size_t i = 0; i + 1 < notes.size(); ++i)
    {
        auto& current = notes[i];
        const auto& next = notes[i + 1];
        const double gap = next.start - (current.start + current.length);

        if (current.pitch != next.pitch && std::abs(next.pitch - current.pitch) <= 12
            && gap <= 0.5 && uniform(rng) < chance)
        {
            current.length = next.start - current.start;
            current.glideToNext = true;
        }
    }
}

void ArrangementGenerator::humanizeTiming(PartNotes& notes, std::mt19937& rng, double ms, double tempo)
{
    if (ms < 0.4)
        return;
    const double amount = ms * tempo / 60000.0;
    for (auto& note : notes)
    {
        const double beatFrac = note.start - std::floor(note.start + 1.0e-9);
        const bool downbeat = beatFrac < 0.06;
        const bool strong = downbeat || (beatFrac > 0.46 && beatFrac < 0.54);
        const double scale = downbeat ? 0.18 : strong ? 0.35 : 1.05;
        // Played feel: slightly late more often than early.
        const double bipolar = uniform(rng) < 0.38 ? -uniform(rng) : uniform(rng);
        note.start = juce::jmax(0.0, note.start + bipolar * amount * scale);
    }
}

void ArrangementGenerator::liftAboveBass(PartNotes& notes, int bassHigh)
{
    const int floorPitch = bassHigh + 3;
    for (auto& note : notes)
        while (note.pitch < floorPitch && note.pitch + 12 <= 88)
            note.pitch += 12;
}

void ArrangementGenerator::applySwing(PartNotes& notes, double swing, bool sixteenths)
{
    if (swing <= 0.5 + 1.0e-6)
        return;

    const double unit = sixteenths ? 0.5 : 1.0;
    auto warp = [=](double beat)
    {
        const double base = std::floor(beat / unit + 1.0e-9) * unit;
        const double fraction = (beat - base) / unit;
        const double warped = fraction < 0.5 ? fraction * (swing / 0.5)
                                             : swing + (fraction - 0.5) * ((1.0 - swing) / 0.5);
        return base + warped * unit;
    };

    for (auto& note : notes)
    {
        const double end = warp(note.start + note.length);
        note.start = warp(note.start);
        note.length = juce::jmax(0.05, end - note.start);
    }
}

void ArrangementGenerator::finalise(PartNotes& notes, double totalBeats)
{
    notes.erase(std::remove_if(notes.begin(), notes.end(),
                               [=](const PartNote& n) { return n.start >= totalBeats - 1.0e-6; }),
                notes.end());

    for (auto& note : notes)
    {
        note.length = juce::jmin(note.length, totalBeats - note.start);
        note.pitch = juce::jlimit(0, 127, note.pitch);
    }

    std::sort(notes.begin(), notes.end(), [](const PartNote& a, const PartNote& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });

    // Same pitch overlapping itself: end the earlier note where the next one starts.
    for (size_t i = 0; i < notes.size(); ++i)
    {
        for (size_t j = i + 1; j < notes.size(); ++j)
        {
            if (notes[j].start >= notes[i].start + notes[i].length)
                break;
            if (notes[j].pitch == notes[i].pitch)
            {
                notes[i].length = notes[j].start - notes[i].start;
                break;
            }
        }
    }

    notes.erase(std::remove_if(notes.begin(), notes.end(),
                               [](const PartNote& n) { return n.length < 0.02; }),
                notes.end());
}

void ArrangementGenerator::appendToBuffer(const PartNotes& notes, int channel, int bendRangeSemitones,
                                          juce::MidiBuffer& buffer)
{
    struct Event
    {
        int tick;
        int order;  // note-offs before pitch bends before note-ons at the same tick
        juce::MidiMessage message;
    };

    std::vector<Event> events;
    auto toTick = [](double beat) { return static_cast<int>(std::round(beat * ticksPerBeat)); };

    const bool hasGlides = std::any_of(notes.begin(), notes.end(), [](const PartNote& n) { return n.glideToNext; });
    if (hasGlides)
    {
        // RPN 0: pitch-bend sensitivity, so receiving synths glide by the right interval
        const int rpn[][2] = { { 101, 0 }, { 100, 0 }, { 6, bendRangeSemitones }, { 38, 0 }, { 101, 127 }, { 100, 127 } };
        for (const auto& cc : rpn)
            events.push_back({ 0, -1, juce::MidiMessage::controllerEvent(channel, cc[0], cc[1]) });
    }

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto& note = notes[i];
        const int onTick = toTick(note.start);
        const int offTick = juce::jmax(onTick + 1, toTick(note.start + note.length));
        const auto velocity = static_cast<juce::uint8>(juce::jlimit(1, 127, note.velocity));

        if (note.pitchBend != 8192)
            events.push_back({ onTick, 1, juce::MidiMessage::pitchWheel(channel, note.pitchBend) });

        events.push_back({ onTick, 2, juce::MidiMessage::noteOn(channel, note.pitch, velocity) });
        events.push_back({ offTick, 0, juce::MidiMessage::noteOff(channel, note.pitch) });

        if (note.pitchBend != 8192)
            events.push_back({ offTick, 1, juce::MidiMessage::pitchWheel(channel, 8192) });

        if (note.glideToNext && i + 1 < notes.size())
        {
            const auto& next = notes[i + 1];
            const int nextTick = toTick(next.start);
            const double glideBeats = juce::jmin(0.25, note.length * 0.5);
            const int glideStart = toTick(next.start - glideBeats);
            const int difference = next.pitch - note.pitch;
            constexpr int glideSteps = 8;

            for (int s = 1; s <= glideSteps; ++s)
            {
                const double fraction = s / static_cast<double>(glideSteps);
                const int tick = s == glideSteps ? nextTick - 1
                                                 : glideStart + static_cast<int>((nextTick - glideStart) * fraction);
                const int value = juce::jlimit(0, 16383,
                    8192 + static_cast<int>(std::round(difference * fraction / bendRangeSemitones * 8191.0)));
                events.push_back({ tick, 1, juce::MidiMessage::pitchWheel(channel, value) });
            }
            events.push_back({ nextTick, 1, juce::MidiMessage::pitchWheel(channel, 8192) });
        }
    }

    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b)
    {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    for (const auto& event : events)
        buffer.addEvent(event.message, event.tick);
}
