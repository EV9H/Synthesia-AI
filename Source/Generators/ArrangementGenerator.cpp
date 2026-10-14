#include "ArrangementGenerator.h"
#include "ArrangementPlan.h"
#include "../StyleProfile.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>

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

        if (part != Part::Drums)
        {
            applySwing(notes, swing, swingSixteenths);
            const double humanize = params.humanizeMs > 0.0 ? params.humanizeMs : profile.drums.humanizeMs;
            humanizeTiming(notes, rng, humanize * 0.7, params.tempo);
            if (part == Part::Chords || part == Part::Piano)
                liftAboveBass(notes, profile.bassHigh);
        }
        finalise(notes, ctx.totalBeats);

        if (part == Part::Bass)
            bassNotes = notes;

        if (params.part != Part::All && params.part != part)
            continue;
        if (!partIsEnabled(params, part))
            continue;

        appendToBuffer(notes, partChannel(part), part == Part::Bass ? 12 : 2, result.midi);
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
                const auto voicing = theory::voiceChord(chord, previous, profile.chordVoicing, rng);
                previous = voicing;
                for (size_t i = 0; i < voicing.size(); ++i)
                {
                    const bool top = i + 1 == voicing.size();
                    const float energy = ctx.energyAt(chord.startBeat);
                    notes.push_back({ voicing[i], chord.startBeat, chord.lengthBeats - 0.05,
                                      humanise(juce::roundToInt((top ? 80 : 70) * (0.55f + 0.45f * energy)), rng, 4) });
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

    std::vector<std::vector<int>> voicings;
    std::vector<int> previous;
    for (const auto& chord : progression)
    {
        voicings.push_back(theory::voiceChord(chord, previous, voicingOptions, rng));
        previous = voicings.back();
    }

    PartNotes notes;
    const std::string* pattern = &pick(patterns, rng);
    const std::string* fillPattern = patterns.size() > 1 ? &pick(patterns, rng) : pattern;

    for (int bar = 0; bar < ctx.params.lengthInBars; ++bar)
    {
        const std::string* use = (bar % 8 == 7) ? fillPattern : pattern;

        for (const auto& hit : parsePattern(*use))
        {
            const double start = bar * 4.0 + hit.step * stepBeats;
            double length = hit.length * stepBeats;
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

            const auto& voicing = voicings[static_cast<size_t>(index)];
            size_t from = 0;
            size_t to = voicing.size();
            if (hit.symbol == 'g' && to > 2)
            {
                from = to - 2;
            }
            else if (hit.length >= 4 && to > 3 && uniform(rng) < 0.6)
            {
                from = 1;
            }
            else if (hit.length <= 2 && to > 3 && uniform(rng) < 0.4)
            {
                from = 1;
            }
            if (bar % 8 == 7 && hit.step >= 8 && hit.symbol != 'X')
                continue;
            for (size_t i = from; i < to; ++i)
                notes.push_back({ voicing[i], start, length - 0.02, humanise(velocity, rng, 5) });
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateBass(const Context& ctx, std::mt19937& rng)
{
    const bool walking = ctx.profile.bassStyle == BassStyle::Walking;
    const bool useTwoFeel = walking && !ctx.profile.bassPatterns.empty() && uniform(rng) < 0.35;
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
                    else
                    {
                        const int direction = nextRoot > root ? 1 : -1;
                        pitch = uniform(rng) < 0.7 ? nextRoot - direction : nextRoot - 2 * direction;
                    }
                    break;
                }
                default: break;
            }

            pitch = foldIntoRange(pitch, low - 2, high + 2);

            double length = hit.length * stepBeats;
            length = juce::jmin(length, chordEnd(chord) - start);

            const bool ghost = std::islower(static_cast<unsigned char>(hit.symbol)) != 0;
            const float density = ctx.densityAt(Part::Bass, start);
            if (ghost && density < 0.45f)
                continue;
            if (role != 'R' && hit.step != 0 && density < 0.28f)
                continue;
            const int velocity = ghost ? 62 : hit.step == 0 ? 102 : 92;
            notes.push_back({ pitch, start, length - 0.02, humanise(velocity, rng, 5) });
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
        notes.push_back({ root, chord.startBeat, 0.92, humanise(96, rng, 4) });

        for (int beat = 1; beat < beats; ++beat)
        {
            int pitch;
            if (beat == beats - 1)
            {
                if (nextRoot == current)
                    pitch = scaleMove(scale, current, uniform(rng) < 0.5 ? 1 : -1);
                else
                {
                    const int direction = nextRoot > current ? 1 : -1;
                    pitch = uniform(rng) < 0.6 ? nextRoot - direction
                                               : scaleMove(scale, nextRoot, -direction);
                }
            }
            else
            {
                const int remaining = beats - beat;
                const double ideal = current + (nextRoot - current) / static_cast<double>(remaining + 1);
                std::vector<int> candidates = beat == 2 ? chordTones : scale;
                if (beat != 2)
                    candidates.insert(candidates.end(), chordTones.begin(), chordTones.end());

                double bestScore = 1.0e9;
                pitch = current;
                for (int candidate : candidates)
                {
                    if (candidate == current || std::abs(candidate - current) > 7)
                        continue;
                    const double score = std::abs(candidate - ideal) + 2.0 * uniform(rng);
                    if (score < bestScore)
                    {
                        bestScore = score;
                        pitch = candidate;
                    }
                }
            }

            pitch = foldIntoRange(pitch, low, high);
            notes.push_back({ pitch, chord.startBeat + beat, 0.92,
                              humanise(beat % 2 == 1 ? 82 : 88, rng, 4) });
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
                          humanise(velocity, rng, isHat ? 4 : 3), false, bend });
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
    const int nGrooves = static_cast<int>(drums.grooves.size());
    int grooveIndex = static_cast<int>(rng() % static_cast<unsigned>(nGrooves));

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
        const bool fillBar = liftComing || (bar % 8 == 7);

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
    int bass = 45;

    for (const auto& chord : ctx.progression)
    {
        bass = foldIntoRange(theory::nearestPitchWithClass(chord.rootPitchClass, bass), 52, 64);
        const auto voicing = theory::voiceChord(chord, previous, ctx.profile.pianoVoicing, rng);
        previous = voicing;

        const int fifth = bass + (chord.intervals.size() > 2 ? chord.intervals[2] % 12 : 7);
        const auto v = [&voicing](size_t i) { return voicing[juce::jmin(i, voicing.size() - 1)]; };
        const int sequence[] = { bass, fifth, v(0), v(1), v(2), v(1), v(0), fifth };

        const double end = chordEnd(chord);
        int index = 0;
        for (double beat = chord.startBeat; beat < end - 1.0e-6; beat += 0.5, ++index)
        {
            const int velocity = index % 8 == 0 ? 84 : index % 2 == 0 ? 72 : 64;
            notes.push_back({ sequence[index % 8], beat, juce::jmin(1.5, end - beat),
                              humanise(velocity, rng, 5) });
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
        const auto base = theory::voiceChord(chord, previous, options, rng);
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
            const int pitch = tones[static_cast<size_t>(shape[index % shape.size()]) % tones.size()];
            const double inBeat = beat - std::floor(beat);
            const int velocity = role.velocity + (inBeat < 1.0e-6 ? 10 : std::abs(inBeat - 0.5) < 1.0e-6 ? 3 : -4);
            notes.push_back({ pitch, beat, rate * role.gate, humanise(velocity, rng, 4) });
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
    const int finest = ctx.finestSteps;
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
            const double offbeat = inBar % 4 == 2 ? 0.25 : inBar % 2 == 1 ? 0.12 : 0.0;
            const double chance = step == 0 ? 0.85 : role.density * (0.4 + weight) + role.syncopation * offbeat;
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

    const Rhythm rhythmA = makeRhythm();
    const auto contourA = makeContour(rhythmA.size());
    const auto contourA2 = varyEnding(contourA);
    const Rhythm rhythmB = uniform(rng) < 0.5 ? mutateRhythm(rhythmA) : makeRhythm();
    const auto contourB = makeContour(rhythmB.size());
    const bool contrastInSecondPhrase = uniform(rng) < 0.5;

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
        if (phraseEnd)
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
            if (!strong && !cadenceNote && uniform(rng) > 0.45 + 0.55 * energy * density)
                continue;

            // Call-and-response: piano answers the second bar of each 2-bar cell;
            // synth/lead leaves that bar more open.
            const double beatInTwoBars = std::fmod(start, 8.0);
            if (!cadenceNote)
            {
                if (part == Part::Piano && beatInTwoBars < 4.0 && uniform(rng) < 0.72)
                    continue;
                if (part == Part::Synth && beatInTwoBars >= 4.0 && !strong && uniform(rng) < 0.55)
                    continue;
            }

            int registerShift = 0;
            if (energy < 0.45f && pitch + 12 <= role.high)
                registerShift = -12;
            else if (energy > 0.85f && pitch - 12 >= role.low && uniform(rng) < 0.25)
                registerShift = 12;
            if (registerShift != 0 && pitch + registerShift >= role.low && pitch + registerShift <= role.high)
                pitch += registerShift;

            const int velocity = juce::roundToInt((role.velocity + (strong ? 6 : -4) - (cadenceNote ? 4 : 0))
                                                  * (0.55f + 0.45f * energy));
            const double length = rn.length * stepBeats * (cadenceNote ? 1.0 : 0.92);
            notes.push_back({ pitch, start, length, humanise(velocity, rng, 5) });

            if (role.dyads && strong && uniform(rng) < 0.4)
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
        const double scale = beatFrac < 0.04 ? 0.28 : 1.0;
        note.start = juce::jmax(0.0, note.start + (uniform(rng) * 2.0 - 1.0) * amount * scale);
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
