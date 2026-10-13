#include "MidiImporter.h"
#include "MidiExporter.h"
#include "Music/MusicTheory.h"
#include "Generators/GenreProfiles.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace
{
    constexpr double kTicks = 960.0;

    struct RawNote
    {
        int pitch{60};
        double start{0.0};
        double length{1.0};
        int velocity{80};
        int channel{1};
    };

    bool isDrumPitch(int pitch)
    {
        return pitch >= 35 && pitch <= 81;
    }

    bool channelLooksLikeDrums(int channel, const std::vector<RawNote>& notes)
    {
        if (channel == 10)
            return true;

        int drumHits = 0;
        int total = 0;
        for (const auto& note : notes)
        {
            if (note.channel != channel)
                continue;
            ++total;
            if (isDrumPitch(note.pitch) && note.length <= 0.6)
                ++drumHits;
        }
        return total >= 4 && drumHits * 2 >= total;
    }

    double meanPitch(const std::vector<RawNote>& notes, int channel)
    {
        double sum = 0.0;
        int count = 0;
        for (const auto& note : notes)
        {
            if (note.channel == channel)
            {
                sum += note.pitch;
                ++count;
            }
        }
        return count > 0 ? sum / count : 60.0;
    }

    double polyphony(const std::vector<RawNote>& notes, int channel)
    {
        std::vector<const RawNote*> subset;
        for (const auto& note : notes)
            if (note.channel == channel)
                subset.push_back(&note);
        if (subset.size() < 2)
            return 1.0;

        double overlap = 0.0;
        for (const auto* note : subset)
        {
            int simultaneous = 0;
            const double mid = note->start + note->length * 0.5;
            for (const auto* other : subset)
                if (mid >= other->start && mid < other->start + other->length)
                    ++simultaneous;
            overlap += simultaneous;
        }
        return overlap / static_cast<double>(subset.size());
    }

    std::vector<int> channelsIn(const std::vector<RawNote>& notes)
    {
        std::set<int> unique;
        for (const auto& note : notes)
            unique.insert(note.channel);
        return { unique.begin(), unique.end() };
    }

    Part classifyChannel(int channel, const std::vector<RawNote>& notes)
    {
        if (channelLooksLikeDrums(channel, notes))
            return Part::Drums;

        const double mean = meanPitch(notes, channel);
        const double poly = polyphony(notes, channel);
        if (mean < 52.0 && poly < 1.35)
            return Part::Bass;
        if (poly >= 2.2 || (poly >= 1.6 && mean < 76.0))
            return Part::Chords;
        if (mean >= 70.0)
            return Part::Synth;
        return Part::Piano;
    }

    Part pickPrimary(int presentMask)
    {
        if (isPartLocked(presentMask, Part::Piano))  return Part::Piano;
        if (isPartLocked(presentMask, Part::Synth))  return Part::Synth;
        if (isPartLocked(presentMask, Part::Chords)) return Part::Chords;
        if (isPartLocked(presentMask, Part::Bass))   return Part::Bass;
        return Part::Piano;
    }

    std::array<double, 12> pitchHistogram(const std::vector<RawNote>& notes)
    {
        std::array<double, 12> hist{};
        for (const auto& note : notes)
        {
            if (note.channel == 10)
                continue;
            const int pc = ((note.pitch % 12) + 12) % 12;
            const double weight = juce::jmax(0.15, note.length);
            hist[static_cast<size_t>(pc)] += weight;
        }
        return hist;
    }

    double correlate(const std::array<double, 12>& hist, const std::array<double, 12>& profile, int root)
    {
        double meanH = 0.0, meanP = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            meanH += hist[static_cast<size_t>(i)];
            meanP += profile[static_cast<size_t>(i)];
        }
        meanH /= 12.0;
        meanP /= 12.0;

        double num = 0.0, denH = 0.0, denP = 0.0;
        for (int i = 0; i < 12; ++i)
        {
            const double h = hist[static_cast<size_t>(i)] - meanH;
            const double p = profile[static_cast<size_t>((i - root + 12) % 12)] - meanP;
            num += h * p;
            denH += h * h;
            denP += p * p;
        }
        const double den = std::sqrt(denH * denP);
        return den > 1.0e-9 ? num / den : 0.0;
    }

    void detectKey(const std::vector<RawNote>& notes, int& root, ScaleType& scale)
    {
        const auto hist = pitchHistogram(notes);
        static const std::array<double, 12> major{
            6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88
        };
        static const std::array<double, 12> minor{
            6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17
        };

        double best = -2.0;
        root = 0;
        scale = ScaleType::Major;
        for (int r = 0; r < 12; ++r)
        {
            const double maj = correlate(hist, major, r);
            const double min = correlate(hist, minor, r);
            if (maj > best) { best = maj; root = r; scale = ScaleType::Major; }
            if (min > best) { best = min; root = r; scale = ScaleType::Minor; }
        }

        const auto intervals = theory::melodyIntervals(scale);
        auto inScaleRatio = [&](ScaleType candidate)
        {
            const auto iv = theory::melodyIntervals(candidate);
            double in = 0.0, all = 0.0;
            for (int pc = 0; pc < 12; ++pc)
            {
                const double w = hist[static_cast<size_t>(pc)];
                all += w;
                const int rel = (pc - root + 12) % 12;
                if (std::find(iv.begin(), iv.end(), rel) != iv.end())
                    in += w;
            }
            return all > 0.0 ? in / all : 0.0;
        };

        double bestRatio = inScaleRatio(scale);
        for (auto candidate : { ScaleType::Dorian, ScaleType::Mixolydian, ScaleType::Pentatonic })
        {
            const double ratio = inScaleRatio(candidate);
            if (ratio > bestRatio + 0.04)
            {
                bestRatio = ratio;
                scale = candidate;
            }
        }
        juce::ignoreUnused(intervals);
    }

    struct Template
    {
        std::vector<int> intervals;
    };

    ChordInfo bestChordForBar(const std::array<double, 12>& hist, int keyRoot, ScaleType scale,
                              double start, double length)
    {
        static const Template templates[] = {
            { { 0, 4, 7, 11 } },
            { { 0, 4, 7, 10 } },
            { { 0, 3, 7, 10 } },
            { { 0, 4, 7 } },
            { { 0, 3, 7 } },
            { { 0, 3, 7, 10, 14 } },
            { { 0, 4, 7, 11, 14 } },
            { { 0, 5, 7 } },
            { { 0, 2, 7 } },
            { { 0, 3, 6 } },
            { { 0, 4, 7, 10, 14 } }
        };

        double histSum = 0.0;
        for (double v : hist)
            histSum += v;

        ChordInfo best;
        double bestScore = -1.0e9;

        if (histSum > 1.0e-6)
        {
            for (int root = 0; root < 12; ++root)
            {
                for (const auto& templ : templates)
                {
                    double hit = 0.0;
                    int missing = 0;
                    for (int interval : templ.intervals)
                    {
                        const int pc = (root + interval) % 12;
                        const double w = hist[static_cast<size_t>(pc)];
                        hit += w;
                        if (w < 0.08)
                            ++missing;
                    }
                    double extra = 0.0;
                    for (int pc = 0; pc < 12; ++pc)
                    {
                        bool inChord = false;
                        for (int interval : templ.intervals)
                            if ((root + interval) % 12 == pc)
                                inChord = true;
                        if (!inChord)
                            extra += hist[static_cast<size_t>(pc)];
                    }
                    double score = hit - 0.55 * extra - 0.85 * missing
                                 - 0.04 * static_cast<double>(templ.intervals.size());
                    if (root == keyRoot)
                        score += 0.12 * histSum;
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best.rootPitchClass = root;
                        best.intervals = templ.intervals;
                    }
                }
            }
        }

        if (bestScore < 0.12 * juce::jmax(0.2, histSum))
        {
            int degree = 0;
            double bestDeg = -1.0;
            const auto mode = theory::modeIntervals(scale);
            for (int d = 0; d < 7; ++d)
            {
                const int pc = (keyRoot + mode[static_cast<size_t>(d)]) % 12;
                if (hist[static_cast<size_t>(pc)] > bestDeg)
                {
                    bestDeg = hist[static_cast<size_t>(pc)];
                    degree = d;
                }
            }
            const auto spec = theory::parseChordToken(std::to_string(degree + 1));
            best = theory::buildChord(keyRoot, scale, spec, 7);
        }

        best.startBeat = start;
        best.lengthBeats = length;
        best.symbol = theory::chordSymbol(best.rootPitchClass, best.intervals,
                                          theory::keyPrefersFlats(keyRoot, scale));
        return best;
    }

    std::vector<ChordInfo> detectProgression(const std::vector<RawNote>& notes, int bars,
                                             int keyRoot, ScaleType scale)
    {
        std::vector<ChordInfo> progression;
        if (bars <= 0)
            return progression;

        for (int bar = 0; bar < bars; ++bar)
        {
            std::array<double, 12> hist{};
            const double start = bar * 4.0;
            const double end = start + 4.0;
            for (const auto& note : notes)
            {
                if (note.channel == 10)
                    continue;
                const double overlap = juce::jmin(end, note.start + note.length) - juce::jmax(start, note.start);
                if (overlap <= 0.0)
                    continue;
                double weight = overlap;
                if (note.start < start + 0.1)
                    weight *= 1.6;
                hist[static_cast<size_t>(((note.pitch % 12) + 12) % 12)] += weight;
            }
            progression.push_back(bestChordForBar(hist, keyRoot, scale, start, 4.0));
        }
        return progression;
    }

    std::string chartFromProgression(const std::vector<ChordInfo>& progression)
    {
        std::string out;
        for (size_t i = 0; i < progression.size(); ++i)
        {
            if (i > 0)
                out += " | ";
            out += progression[i].symbol.empty() ? "-" : progression[i].symbol;
        }
        return out;
    }

    void detectMotif(const std::vector<RawNote>& notes, Part melodyPart,
                     std::string& contour, std::string& rhythm)
    {
        std::vector<const RawNote*> melody;
        for (const auto& note : notes)
            if (partFromChannel(note.channel) == melodyPart || note.channel == partChannel(melodyPart))
                melody.push_back(&note);

        if (melody.size() < 3)
        {
            for (const auto& note : notes)
                if (note.channel != 10)
                    melody.push_back(&note);
        }

        std::sort(melody.begin(), melody.end(),
                  [](const RawNote* a, const RawNote* b) { return a->start < b->start; });

        contour = "arch";
        rhythm = "straight";
        if (melody.size() < 3)
            return;

        const int first = melody.front()->pitch;
        const int last = melody.back()->pitch;
        const int mid = melody[melody.size() / 2]->pitch;
        if (last > first + 3 && mid >= first)
            contour = "rising";
        else if (last < first - 3 && mid <= first)
            contour = "falling";
        else if (std::abs(last - first) <= 2 && std::abs(mid - first) <= 3)
            contour = "static";
        else if (mid > first + 2 && mid > last + 2)
            contour = "arch";

        int offbeat = 0;
        for (const auto* note : melody)
        {
            const double frac = std::fmod(note->start, 1.0);
            if (frac > 0.2 && frac < 0.8)
                ++offbeat;
        }
        const double density = static_cast<double>(melody.size())
                             / juce::jmax(1.0, melody.back()->start + melody.back()->length);
        if (density < 0.7)
            rhythm = "sparse";
        else if (offbeat * 2 > static_cast<int>(melody.size()))
            rhythm = "syncopated";
    }

    juce::MidiBuffer toBuffer(const std::vector<RawNote>& notes)
    {
        juce::MidiBuffer buffer;
        for (const auto& note : notes)
        {
            const int start = juce::roundToInt(note.start * kTicks);
            const int end = juce::roundToInt((note.start + juce::jmax(0.05, note.length)) * kTicks);
            buffer.addEvent(juce::MidiMessage::noteOn(note.channel, note.pitch, (juce::uint8) juce::jlimit(1, 127, note.velocity)),
                            start);
            buffer.addEvent(juce::MidiMessage::noteOff(note.channel, note.pitch), juce::jmax(start + 1, end));
        }
        return buffer;
    }

    MidiAnalysis analyseNotes(std::vector<RawNote> notes, double tempo, const juce::String& name)
    {
        MidiAnalysis analysis;
        analysis.sourceName = name;
        analysis.tempo = juce::jlimit(40.0, 240.0, tempo);

        if (notes.empty())
        {
            analysis.error = "No notes in MIDI";
            return analysis;
        }

        double end = 0.0;
        for (const auto& note : notes)
            end = juce::jmax(end, note.start + note.length);
        analysis.lengthInBars = juce::jlimit(1, 32, juce::roundToInt(std::ceil(end / 4.0 - 1.0e-4)));
        analysis.lengthInBeats = analysis.lengthInBars * 4.0;

        detectKey(notes, analysis.rootNote, analysis.scale);
        analysis.progression = detectProgression(notes, analysis.lengthInBars, analysis.rootNote, analysis.scale);
        analysis.chordChart = chartFromProgression(analysis.progression);

        std::map<int, Part> role;
        int present = 0;
        for (int channel : channelsIn(notes))
        {
            const Part part = classifyChannel(channel, notes);
            role[channel] = part;
            present |= partLockBit(part);
        }
        analysis.presentParts = present;
        analysis.primaryPart = pickPrimary(present);

        std::vector<RawNote> remapped = notes;
        for (auto& note : remapped)
        {
            auto it = role.find(note.channel);
            const Part part = it != role.end() ? it->second : analysis.primaryPart;
            note.channel = partChannel(part);
        }
        analysis.remapped = toBuffer(remapped);

        detectMotif(remapped, analysis.primaryPart, analysis.motifContour, analysis.motifRhythm);

        std::ostringstream summary;
        summary << "Imported " << (name.isEmpty() ? "MIDI" : name.toStdString())
                << " · " << theory::pitchClassName(analysis.rootNote,
                    theory::keyPrefersFlats(analysis.rootNote, analysis.scale));
        static const char* scaleNames[] = { "major", "minor", "dorian", "mixolydian", "pentatonic" };
        summary << " " << scaleNames[static_cast<int>(analysis.scale)]
                << " · " << juce::roundToInt(analysis.tempo) << " bpm · "
                << analysis.lengthInBars << " bars\n"
                << "Primary: " << partName(analysis.primaryPart)
                << " · motif " << analysis.motifContour << "/" << analysis.motifRhythm << "\n"
                << analysis.chordChart;
        analysis.summary = summary.str();
        analysis.ok = true;
        return analysis;
    }

    std::vector<RawNote> notesFromBuffer(const juce::MidiBuffer& midi, double ticksPerQuarter)
    {
        struct Open { double start; int velocity; };
        std::map<std::pair<int, int>, Open> open;
        std::vector<RawNote> notes;
        const double scale = ticksPerQuarter > 0.0 ? (1.0 / ticksPerQuarter) : (1.0 / kTicks);

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            const double beat = metadata.samplePosition * scale;
            const auto key = std::make_pair(message.getChannel(), message.getNoteNumber());

            if (message.isNoteOn())
            {
                auto it = open.find(key);
                if (it != open.end())
                {
                    notes.push_back({ key.second, it->second.start, juce::jmax(0.05, beat - it->second.start),
                                      it->second.velocity, key.first });
                    open.erase(it);
                }
                open[key] = { beat, message.getVelocity() };
            }
            else if (message.isNoteOff())
            {
                auto it = open.find(key);
                if (it != open.end())
                {
                    notes.push_back({ key.second, it->second.start, juce::jmax(0.05, beat - it->second.start),
                                      it->second.velocity, key.first });
                    open.erase(it);
                }
            }
        }
        return notes;
    }

    bool writeSimpleFile(const juce::File& file, const std::vector<RawNote>& notes, double tempo)
    {
        return MidiExporter::exportToFile(toBuffer(notes), file, tempo);
    }
}

MidiAnalysis MidiImporter::analyseFile(const juce::File& file)
{
    MidiAnalysis failed;
    failed.sourceName = file.getFileName();
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        failed.error = "Could not open " + file.getFullPathName();
        return failed;
    }

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream, true))
    {
        failed.error = "Not a valid MIDI file";
        return failed;
    }

    const short timeFormat = midiFile.getTimeFormat();
    const double tpq = timeFormat > 0 ? static_cast<double>(timeFormat) : 96.0;

    double tempo = 120.0;
    juce::MidiBuffer combined;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack(t);
        if (track == nullptr)
            continue;
        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            auto message = track->getEventPointer(i)->message;
            if (message.isTempoMetaEvent())
                tempo = juce::jlimit(40.0, 240.0, 60.0 / message.getTempoSecondsPerQuarterNote());
            const int tick = juce::roundToInt(message.getTimeStamp() * kTicks / tpq);
            combined.addEvent(message, juce::jmax(0, tick));
        }
    }

    return analyseBuffer(combined, kTicks, tempo, file.getFileName());
}

MidiAnalysis MidiImporter::analyseBuffer(const juce::MidiBuffer& midi, double ticksPerQuarter, double tempo,
                                         const juce::String& sourceName)
{
    return analyseNotes(notesFromBuffer(midi, ticksPerQuarter), tempo, sourceName);
}

juce::MidiBuffer MidiImporter::overlay(const juce::MidiBuffer& generated, const MidiAnalysis& imported,
                                       ImportMode mode, int extraKeepMask, double generatedBeats)
{
    if (!imported.ok || mode == ImportMode::None || mode == ImportMode::Rewrite)
        return generated;

    int keep = extraKeepMask;
    if (mode == ImportMode::Restyle)
        keep |= partLockBit(imported.primaryPart);
    else
        keep |= imported.presentParts != 0 ? imported.presentParts : partLockBit(imported.primaryPart);

    const double srcBeats = imported.lengthInBeats;
    juce::MidiBuffer out;

    for (const auto part : allParts)
    {
        if (!isPartLocked(keep, part))
        {
            out.addEvents(MidiExporter::filterPart(generated, part), 0, -1, 0);
            continue;
        }

        const auto importedPart = MidiExporter::filterPart(imported.remapped, part);
        if (mode == ImportMode::Extend)
        {
            out.addEvents(importedPart, 0, -1, 0);
            for (const auto metadata : MidiExporter::filterPart(generated, part))
            {
                const double beat = metadata.samplePosition / kTicks;
                if (beat >= srcBeats - 1.0e-4)
                    out.addEvent(metadata.getMessage(), metadata.samplePosition);
            }
        }
        else
        {
            out.addEvents(importedPart, 0, -1, 0);
            if (generatedBeats > srcBeats + 0.25)
            {
                for (const auto metadata : MidiExporter::filterPart(generated, part))
                {
                    const double beat = metadata.samplePosition / kTicks;
                    if (beat >= srcBeats - 1.0e-4)
                        out.addEvent(metadata.getMessage(), metadata.samplePosition);
                }
            }
        }
    }

    juce::ignoreUnused(generatedBeats);
    return out;
}

bool MidiImporter::writePianoMelody(const juce::File& file)
{
    const int ch = partChannel(Part::Piano);
    const int vel = 86;
    std::vector<RawNote> notes;
    auto add = [&](int bar, int beat, int pitch, double length)
    {
        notes.push_back({ pitch, bar * 4.0 + beat, length, vel, ch });
    };

    // 8 bars, C major, C | Am | F | G | C | Am | F | G  (quarters)
    const int phrases[8][4] = {
        { 60, 64, 67, 64 },
        { 57, 60, 64, 60 },
        { 65, 69, 72, 69 },
        { 67, 71, 74, 67 },
        { 72, 67, 64, 60 },
        { 64, 60, 57, 52 },
        { 65, 69, 72, 76 },
        { 74, 71, 67, 62 }
    };
    for (int bar = 0; bar < 8; ++bar)
        for (int beat = 0; beat < 4; ++beat)
            add(bar, beat, phrases[bar][beat], 0.95);

    return writeSimpleFile(file, notes, 110.0);
}

bool MidiImporter::writeChordLoop(const juce::File& file)
{
    const int ch = partChannel(Part::Chords);
    struct Voicing { int pitches[4]; };
    const Voicing voicings[4] = {
        { { 48, 52, 55, 59 } }, // Cmaj7
        { { 45, 48, 52, 55 } }, // Am7
        { { 41, 45, 48, 52 } }, // Fmaj7
        { { 43, 47, 50, 53 } }  // G7
    };

    std::vector<RawNote> notes;
    for (int bar = 0; bar < 8; ++bar)
    {
        const auto& v = voicings[bar % 4];
        for (int p : v.pitches)
            notes.push_back({ p, bar * 4.0, 3.9, 78, ch });
    }
    return writeSimpleFile(file, notes, 110.0);
}
