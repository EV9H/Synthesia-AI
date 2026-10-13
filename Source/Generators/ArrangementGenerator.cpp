#include "ArrangementGenerator.h"
#include <cctype>
#include <cmath>
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
    const auto& profile = getGenreProfile(params.genre);
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

GenerationResult ArrangementGenerator::generateArrangement(const MidiGenerationParams& params)
{
    const auto& profile = getGenreProfile(params.genre);
    const unsigned int seed = params.seed != 0 ? params.seed : std::random_device{}();

    GenerationResult result;
    if (params.progression.empty())
    {
        auto progressionRng = streamFor(seed, 1);
        result.progression = buildProgression(params, progressionRng);
    }
    else
    {
        result.progression = params.progression;
    }

    const Context ctx{ params, profile, result.progression, params.lengthInBars * 4.0,
                       gridSteps(params.quantization) };

    for (const auto part : allParts)
    {
        if (params.part != Part::All && params.part != part)
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
            case Part::All:    break;
        }

        applySwing(notes, profile.swing, profile.swingSixteenths);
        finalise(notes, ctx.totalBeats);
        appendToBuffer(notes, partChannel(part), part == Part::Bass ? 12 : 2, result.midi);
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
                    notes.push_back({ voicing[i], chord.startBeat, chord.lengthBeats - 0.05,
                                      humanise(top ? 80 : 70, rng, 4) });
                }
            }
            return notes;
        }

        case ChordStyle::Arpeggiated:
            return generateArp(ctx, profile.chordArp, juce::jmax(1, ctx.finestSteps), rng);

        case ChordStyle::Pulse:
        case ChordStyle::Stabs:
        case ChordStyle::Comping:
        default:
            return generatePatternChords(ctx, profile.chordPatterns, profile.chordVoicing,
                                         profile.chordStyle == ChordStyle::Stabs ? 88 : 80, rng);
    }
}

ArrangementGenerator::PartNotes ArrangementGenerator::generatePatternChords(
    const Context& ctx, const std::vector<std::string>& patterns,
    const theory::VoicingOptions& voicingOptions, int baseVelocity, std::mt19937& rng)
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

    for (int bar = 0; bar < ctx.params.lengthInBars; ++bar)
    {
        if (bar % 2 == 0)
            pattern = &pick(patterns, rng);

        for (const auto& hit : parsePattern(*pattern))
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

            const int velocity = hit.symbol == 'X' ? baseVelocity + 10
                               : hit.symbol == 'g' ? baseVelocity - 25
                                                   : baseVelocity;

            for (int pitch : voicings[static_cast<size_t>(index)])
                notes.push_back({ pitch, start, length - 0.02, humanise(velocity, rng, 5) });
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateBass(const Context& ctx, std::mt19937& rng)
{
    auto notes = ctx.profile.bassStyle == BassStyle::Walking ? generateWalkingBass(ctx, rng)
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
    const std::string* pattern = &pick(profile.bassPatterns, rng);

    for (int bar = 0; bar < ctx.params.lengthInBars; ++bar)
    {
        if (bar % 4 == 3 && !approachPatterns.empty())
            pattern = &pick(approachPatterns, rng);
        else if (bar % 2 == 0)
            pattern = &pick(profile.bassPatterns, rng);

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
        return generateArp(ctx, ctx.profile.leadArp, ctx.finestSteps, rng);

    return generateMelody(ctx, ctx.profile.leadMelody, rng);
}

ArrangementGenerator::PartNotes ArrangementGenerator::generatePiano(const Context& ctx, std::mt19937& rng)
{
    const auto& profile = ctx.profile;

    switch (profile.pianoStyle)
    {
        case PianoStyle::Broken:   return generateBrokenPiano(ctx, rng);
        case PianoStyle::Comp:     return generatePatternChords(ctx, profile.pianoPatterns, profile.pianoVoicing, 82, rng);
        case PianoStyle::Melodic:  return generateMelody(ctx, profile.pianoMelody, rng);
        case PianoStyle::Arpeggio: return generateArp(ctx, profile.pianoArp, ctx.finestSteps, rng);
    }

    return {};
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateBrokenPiano(const Context& ctx, std::mt19937& rng)
{
    PartNotes notes;
    std::vector<int> previous;
    int bass = 45;

    for (const auto& chord : ctx.progression)
    {
        bass = foldIntoRange(theory::nearestPitchWithClass(chord.rootPitchClass, bass), 40, 52);
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
                                                                  int rateSteps, std::mt19937& rng)
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
            const int pitch = tones[static_cast<size_t>(shape[index % shape.size()]) % tones.size()];
            const double inBeat = beat - std::floor(beat);
            const int velocity = role.velocity + (inBeat < 1.0e-6 ? 10 : std::abs(inBeat - 0.5) < 1.0e-6 ? 3 : -4);
            notes.push_back({ pitch, beat, rate * role.gate, humanise(velocity, rng, 4) });
        }
    }

    return notes;
}

ArrangementGenerator::PartNotes ArrangementGenerator::generateMelody(const Context& ctx, const MelodyProfile& role,
                                                                     std::mt19937& rng)
{
    struct RhythmNote
    {
        int step;
        int length;
    };
    using Rhythm = std::vector<RhythmNote>;

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
            const double upChance = i < count / 2 ? 0.62 : 0.38;
            const int direction = uniform(rng) < upChance ? 1 : -1;
            int delta;

            if (std::abs(previous) >= 3)
                delta = previous > 0 ? -1 : 1;     // fill the gap after a leap
            else
            {
                const double r = uniform(rng);
                delta = r < 0.12 ? direction * (3 + static_cast<int>(rng() % 2))
                      : r < 0.25 ? 0
                      : r < 0.65 ? direction
                                 : direction * 2;
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

            const int velocity = role.velocity + (strong ? 6 : -4) - (cadenceNote ? 4 : 0);
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

        events.push_back({ onTick, 2, juce::MidiMessage::noteOn(channel, note.pitch, velocity) });
        events.push_back({ offTick, 0, juce::MidiMessage::noteOff(channel, note.pitch) });

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
