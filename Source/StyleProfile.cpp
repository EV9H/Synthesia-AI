#include "StyleProfile.h"
#include "MidiExporter.h"
#include "Music/MusicTheory.h"
#include <algorithm>
#include <cmath>
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

    double lerp(double a, double b, double t) { return a + (b - a) * t; }
    int ilerp(int a, int b, double t) { return juce::roundToInt(lerp(static_cast<double>(a),
                                                                     static_cast<double>(b), t)); }

    std::vector<RawNote> notesFromBuffer(const juce::MidiBuffer& midi)
    {
        struct Open { double start; int velocity; };
        std::map<std::pair<int, int>, Open> open;
        std::vector<RawNote> notes;

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            const double beat = metadata.samplePosition / kTicks;
            const auto key = std::make_pair(message.getChannel(), message.getNoteNumber());

            if (message.isNoteOn())
            {
                auto it = open.find(key);
                if (it != open.end())
                {
                    notes.push_back({ key.second, it->second.start,
                                      juce::jmax(0.05, beat - it->second.start),
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
                    notes.push_back({ key.second, it->second.start,
                                      juce::jmax(0.05, beat - it->second.start),
                                      it->second.velocity, key.first });
                    open.erase(it);
                }
            }
        }
        return notes;
    }

    juce::MidiBuffer toBuffer(const std::vector<RawNote>& notes)
    {
        juce::MidiBuffer buffer;
        for (const auto& note : notes)
        {
            const int start = juce::roundToInt(note.start * kTicks);
            const int end = juce::roundToInt((note.start + juce::jmax(0.05, note.length)) * kTicks);
            buffer.addEvent(juce::MidiMessage::noteOn(note.channel, note.pitch,
                                                      (juce::uint8) juce::jlimit(1, 127, note.velocity)),
                            start);
            buffer.addEvent(juce::MidiMessage::noteOff(note.channel, note.pitch), juce::jmax(start + 1, end));
        }
        return buffer;
    }

    int closestDegree(int relativePc, ScaleType scale, bool& borrowed)
    {
        borrowed = false;
        const auto mode = theory::modeIntervals(scale);
        int best = 0;
        int bestDist = 12;
        for (int d = 0; d < 7; ++d)
        {
            const int dist = std::min(std::abs(mode[static_cast<size_t>(d)] - relativePc),
                                      12 - std::abs(mode[static_cast<size_t>(d)] - relativePc));
            if (dist < bestDist)
            {
                bestDist = dist;
                best = d;
            }
        }
        if (bestDist > 0)
        {
            static const std::array<int, 7> major{ 0, 2, 4, 5, 7, 9, 11 };
            for (int d = 0; d < 7; ++d)
            {
                const int flat = (major[static_cast<size_t>(d)] + 11) % 12;
                if (flat == relativePc)
                {
                    borrowed = true;
                    return d;
                }
            }
        }
        return best;
    }

    std::string chordToToken(const ChordInfo& chord, int keyRoot, ScaleType scale)
    {
        const int relative = (chord.rootPitchClass - keyRoot + 12) % 12;
        bool borrowed = false;
        const int degree = closestDegree(relative, scale, borrowed);

        std::set<int> tones;
        for (int interval : chord.intervals)
            tones.insert(((interval % 12) + 12) % 12);

        const bool maj3 = tones.count(4) > 0;
        const bool min7 = tones.count(10) > 0;
        const bool maj7 = tones.count(11) > 0;
        const bool ninth = tones.count(2) > 0;
        const bool eleventh = tones.count(5) > 0 && maj3;

        int ext = 3;
        if (min7 || maj7) ext = 7;
        if (ninth) ext = 9;
        if (eleventh) ext = 11;

        const bool dominant = maj3 && min7;
        std::string token;
        if (borrowed)
            token += "b";
        token += static_cast<char>('1' + degree);
        if (dominant)
            token += "D";
        if (ext == 3) token += "t";
        else if (ext == 7) token += "7";
        else if (ext == 9) token += "9";
        else token += "11";
        return token;
    }

    char bassRole(int pitch, const ChordInfo& chord, const ChordInfo& next)
    {
        const int pc = ((pitch % 12) + 12) % 12;
        const int root = chord.rootPitchClass;
        const int nextRoot = next.rootPitchClass;
        if (pc == root) return 'R';
        if (pc == (root + 7) % 12) return 'F';
        if (pc == (root + 12) % 12) return 'O';
        if (pc == (root + 3) % 12 || pc == (root + 4) % 12) return 'T';
        const int distNext = std::min(std::abs(pc - nextRoot), 12 - std::abs(pc - nextRoot));
        if (distNext > 0 && distNext <= 2) return 'A';
        return 'r';
    }

    std::string gridToPattern(const std::array<double, 16>& grid, bool bass)
    {
        double peak = 0.0;
        for (double v : grid)
            peak = juce::jmax(peak, v);
        if (peak < 1.0e-6)
            return {};

        std::string out(16, '.');
        for (int i = 0; i < 16; ++i)
        {
            const double n = grid[static_cast<size_t>(i)] / peak;
            if (n < 0.18)
                continue;
            if (bass)
                out[static_cast<size_t>(i)] = n > 0.55 ? 'R' : 'r';
            else if (n > 0.7)
                out[static_cast<size_t>(i)] = 'X';
            else if (n > 0.35)
                out[static_cast<size_t>(i)] = 'x';
            else
                out[static_cast<size_t>(i)] = 'g';
        }
        return out;
    }

    void addPattern(std::vector<std::string>& patterns, const std::string& pattern)
    {
        if (pattern.empty() || pattern.find_first_not_of('.') == std::string::npos)
            return;
        if (std::find(patterns.begin(), patterns.end(), pattern) == patterns.end())
            patterns.push_back(pattern);
    }

    juce::Array<juce::var> doublesToVar(const std::array<double, 16>& grid)
    {
        juce::Array<juce::var> arr;
        for (double v : grid)
            arr.add(v);
        return arr;
    }

    juce::Array<juce::var> intervalsToVar(const std::array<double, 13>& grid)
    {
        juce::Array<juce::var> arr;
        for (double v : grid)
            arr.add(v);
        return arr;
    }

    void varToGrid16(const juce::var& value, std::array<double, 16>& grid)
    {
        if (auto* arr = value.getArray())
            for (int i = 0; i < juce::jmin(16, arr->size()); ++i)
                grid[static_cast<size_t>(i)] = static_cast<double>((*arr)[i]);
    }

    void varToGrid13(const juce::var& value, std::array<double, 13>& grid)
    {
        if (auto* arr = value.getArray())
            for (int i = 0; i < juce::jmin(13, arr->size()); ++i)
                grid[static_cast<size_t>(i)] = static_cast<double>((*arr)[i]);
    }

    juce::DynamicObject::Ptr partToObj(const PartStyleStats& part)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("onsetGrid", doublesToVar(part.onsetGrid));
        obj->setProperty("density", part.density);
        obj->setProperty("velocityMean", part.velocityMean);
        obj->setProperty("registerLow", part.registerLow);
        obj->setProperty("registerHigh", part.registerHigh);
        return obj;
    }

    PartStyleStats partFromVar(const juce::var& value)
    {
        PartStyleStats part;
        if (auto* obj = value.getDynamicObject())
        {
            varToGrid16(obj->getProperty("onsetGrid"), part.onsetGrid);
            part.density = static_cast<double>(obj->getProperty("density"));
            part.velocityMean = static_cast<double>(obj->getProperty("velocityMean"));
            part.registerLow = static_cast<int>(obj->getProperty("registerLow"));
            part.registerHigh = static_cast<int>(obj->getProperty("registerHigh"));
        }
        return part;
    }

    const ChordInfo* chordAt(const std::vector<ChordInfo>& progression, double beat)
    {
        if (progression.empty())
            return nullptr;
        const ChordInfo* found = &progression.front();
        for (const auto& chord : progression)
            if (beat + 1.0e-6 >= chord.startBeat)
                found = &chord;
        return found;
    }

    void accumulatePart(PartStyleStats& stats, const std::vector<RawNote>& notes, Part part)
    {
        const int channel = partChannel(part);
        int count = 0;
        double vel = 0.0;
        for (const auto& note : notes)
        {
            if (note.channel != channel)
                continue;
            const int step = juce::jlimit(0, 15, juce::roundToInt(std::fmod(note.start, 4.0) * 4.0));
            stats.onsetGrid[static_cast<size_t>(step)] += 1.0;
            stats.registerLow = juce::jmin(stats.registerLow, note.pitch);
            stats.registerHigh = juce::jmax(stats.registerHigh, note.pitch);
            vel += note.velocity;
            ++count;
        }
        if (count > 0)
            stats.velocityMean = vel / count;
    }

    void mergePart(PartStyleStats& into, const PartStyleStats& add, double wInto, double wAdd)
    {
        const double w = wInto + wAdd;
        if (w <= 0.0)
            return;
        for (int i = 0; i < 16; ++i)
            into.onsetGrid[static_cast<size_t>(i)] = into.onsetGrid[static_cast<size_t>(i)] * wInto
                                                   + add.onsetGrid[static_cast<size_t>(i)] * wAdd;
        into.density = (into.density * wInto + add.density * wAdd) / w;
        into.velocityMean = (into.velocityMean * wInto + add.velocityMean * wAdd) / w;
        if (add.registerHigh >= add.registerLow)
        {
            into.registerLow = juce::jmin(into.registerLow, add.registerLow);
            into.registerHigh = juce::jmax(into.registerHigh, add.registerHigh);
        }
    }

    void lerpMelody(MelodyProfile& out, const MelodyProfile& genre, const PartStyleStats& stats,
                    const StyleProfile& style, double t)
    {
        if (stats.registerHigh > stats.registerLow)
        {
            out.low = ilerp(genre.low, stats.registerLow, t);
            out.high = ilerp(genre.high, stats.registerHigh, t);
        }
        const double dens = juce::jlimit(0.15, 0.95, stats.density * 0.55);
        if (stats.density > 0.0)
            out.density = lerp(genre.density, dens, t);
        out.velocity = ilerp(genre.velocity, juce::roundToInt(stats.velocityMean), t);
        out.useExtensions = t > 0.45 ? style.defaultExtension >= 9 : genre.useExtensions;
        double intervalSum = 0.0;
        for (double v : style.melodyIntervals)
            intervalSum += v;
        if (intervalSum > 1.0e-6 && t > 0.3)
            out.intervalWeights = style.melodyIntervals;
    }
}

StyleProfile StyleProfile::extract(const MidiAnalysis& analysis)
{
    StyleProfile profile;
    if (!analysis.ok)
        return profile;

    profile.filesLearned = 1;
    profile.sources.push_back(analysis.sourceName.toStdString());
    profile.tempo = analysis.tempo;
    profile.rootNote = analysis.rootNote;
    profile.scale = analysis.scale;
    profile.motifContour = analysis.motifContour;
    profile.motifRhythm = analysis.motifRhythm;

    const auto notes = notesFromBuffer(analysis.remapped);
    double end = 0.0;
    for (const auto& note : notes)
        end = juce::jmax(end, note.start + note.length);
    const double beats = juce::jmax(1.0, analysis.lengthInBeats > 0.0 ? analysis.lengthInBeats : end);

    accumulatePart(profile.chords, notes, Part::Chords);
    accumulatePart(profile.bass, notes, Part::Bass);
    accumulatePart(profile.synth, notes, Part::Synth);
    accumulatePart(profile.piano, notes, Part::Piano);
    accumulatePart(profile.drums, notes, Part::Drums);

    auto densityOf = [&](Part part)
    {
        int count = 0;
        for (const auto& note : notes)
            if (note.channel == partChannel(part))
                ++count;
        return static_cast<double>(count) / beats;
    };
    profile.chords.density = densityOf(Part::Chords);
    profile.bass.density = densityOf(Part::Bass);
    profile.synth.density = densityOf(Part::Synth);
    profile.piano.density = densityOf(Part::Piano);
    profile.drums.density = densityOf(Part::Drums);

    int extSum = 0, extN = 0, spreadSum = 0, spreadN = 0, maxNotes = 0, rootlessHits = 0;
    profile.voicingLow = 127;
    profile.voicingHigh = 0;
    profile.defaultExtension = 0;
    std::string prevToken;
    const bool harmonicSource = isPartLocked(analysis.presentParts, Part::Chords);
    for (const auto& chord : analysis.progression)
    {
        if (!harmonicSource)
            break;
        const auto token = chordToToken(chord, analysis.rootNote, analysis.scale);
        if (std::find(profile.chordVocabulary.begin(), profile.chordVocabulary.end(), token)
            == profile.chordVocabulary.end())
            profile.chordVocabulary.push_back(token);
        if (!prevToken.empty())
            profile.chordTransitions[prevToken][token] += 1.0;
        prevToken = token;

        int ext = 3;
        bool has9 = false, has11 = false, has7 = false;
        for (int interval : chord.intervals)
        {
            const int pc = ((interval % 12) + 12) % 12;
            if (pc == 10 || pc == 11) has7 = true;
            if (pc == 2) has9 = true;
            if (pc == 5) has11 = true;
        }
        if (has7) ext = 7;
        if (has9) ext = 9;
        if (has11) ext = 11;
        extSum += ext;
        ++extN;
    }
    if (extN > 0)
        profile.defaultExtension = juce::jlimit(7, 11, extSum / extN);

    std::map<int, std::vector<const RawNote*>> chordHits;
    for (const auto& note : notes)
        if (note.channel == partChannel(Part::Chords) || note.channel == partChannel(Part::Piano))
            chordHits[juce::roundToInt(note.start * 4.0)].push_back(&note);

    for (const auto& item : chordHits)
    {
        if (item.second.size() < 2)
            continue;
        int lo = 127, hi = 0;
        bool hasRoot = false;
        const auto* chord = chordAt(analysis.progression, item.second.front()->start);
        for (const auto* note : item.second)
        {
            lo = juce::jmin(lo, note->pitch);
            hi = juce::jmax(hi, note->pitch);
            if (chord != nullptr && ((note->pitch % 12) + 12) % 12 == chord->rootPitchClass)
                hasRoot = true;
        }
        spreadSum += hi - lo;
        ++spreadN;
        maxNotes = juce::jmax(maxNotes, static_cast<int>(item.second.size()));
        if (!hasRoot && item.second.size() >= 3)
            ++rootlessHits;
        profile.voicingLow = juce::jmin(profile.voicingLow, lo);
        profile.voicingHigh = juce::jmax(profile.voicingHigh, hi);
    }
    if (spreadN > 0)
        profile.voicingSpread = spreadSum / static_cast<double>(spreadN);
    profile.voicingMaxNotes = juce::jmax(3, maxNotes);
    profile.rootless = rootlessHits * 2 > spreadN && spreadN >= 2;

    // Swing: average position of the second eighth within each beat.
    double swingNum = 0.0, swingDen = 0.0, lateMs = 0.0;
    int lateN = 0;
    int off16 = 0, allOnsets = 0;
    for (const auto& note : notes)
    {
        if (note.channel == 10)
            continue;
        ++allOnsets;
        const double beat = std::fmod(note.start, 1.0);
        const double nearest = std::round(note.start * 4.0) / 4.0;
        const double delayBeats = note.start - nearest;
        lateMs += delayBeats * 60.0 / juce::jmax(40.0, analysis.tempo) * 1000.0;
        ++lateN;
        const double frac16 = std::fmod(note.start, 0.25);
        if (frac16 > 0.04 && frac16 < 0.21)
            ++off16;
        if (beat > 0.42 && beat < 0.82)
        {
            swingNum += beat;
            swingDen += 1.0;
        }
    }
    if (swingDen >= 4.0)
        profile.swing = juce::jlimit(0.5, 0.75, swingNum / swingDen);
    else
        profile.swing = 0.5;
    profile.swingSixteenths = allOnsets > 0 && off16 * 3 > allOnsets;
    if (lateN > 0)
        profile.laidBackMs = juce::jlimit(0.0, 80.0, lateMs / lateN);

    int approaches = 0, bassNotes = 0;
    std::string barPattern(16, '.');
    int currentBar = -1;
    std::vector<RawNote> bassOnly;
    for (const auto& note : notes)
        if (note.channel == partChannel(Part::Bass))
            bassOnly.push_back(note);
    std::sort(bassOnly.begin(), bassOnly.end(),
              [](const RawNote& a, const RawNote& b) { return a.start < b.start; });

    for (size_t i = 0; i < bassOnly.size(); ++i)
    {
        const auto& note = bassOnly[i];
        ++bassNotes;
        const int bar = static_cast<int>(std::floor(note.start / 4.0));
        if (bar != currentBar)
        {
            if (currentBar >= 0)
                addPattern(profile.bassPatterns, barPattern);
            barPattern.assign(16, '.');
            currentBar = bar;
        }
        const int step = juce::jlimit(0, 15, juce::roundToInt(std::fmod(note.start, 4.0) * 4.0));
        const auto* chord = chordAt(analysis.progression, note.start);
        const auto* next = chord;
        if (chord != nullptr)
        {
            for (const auto& candidate : analysis.progression)
                if (candidate.startBeat > chord->startBeat + 1.0e-6)
                {
                    next = &candidate;
                    break;
                }
            if (next == chord && !analysis.progression.empty())
                next = &analysis.progression.front();
            const char role = bassRole(note.pitch, *chord, *next);
            barPattern[static_cast<size_t>(step)] = role;
            if (role == 'A')
                ++approaches;
        }
    }
    if (currentBar >= 0)
        addPattern(profile.bassPatterns, barPattern);
    profile.chromaticApproach = bassNotes > 0 ? approaches / static_cast<double>(bassNotes) : 0.0;

    addPattern(profile.chordPatterns, gridToPattern(profile.chords.onsetGrid, false));
    addPattern(profile.pianoPatterns, gridToPattern(profile.piano.onsetGrid, false));

    std::vector<RawNote> melody;
    const int melodyCh = partChannel(analysis.primaryPart == Part::Bass ? Part::Piano : analysis.primaryPart);
    for (const auto& note : notes)
        if (note.channel == melodyCh || note.channel == partChannel(Part::Synth)
            || (analysis.primaryPart == Part::Piano && note.channel == partChannel(Part::Piano)))
            melody.push_back(note);
    std::sort(melody.begin(), melody.end(),
              [](const RawNote& a, const RawNote& b) { return a.start < b.start; });
    for (size_t i = 1; i < melody.size(); ++i)
    {
        const int delta = juce::jlimit(-6, 6, melody[i].pitch - melody[i - 1].pitch);
        profile.melodyIntervals[static_cast<size_t>(delta + 6)] += 1.0;
    }

    if (profile.voicingHigh < profile.voicingLow)
    {
        profile.voicingLow = 48;
        profile.voicingHigh = 76;
    }

    profile.finaliseSummary();
    return profile;
}

void StyleProfile::absorb(const StyleProfile& other)
{
    if (other.empty())
        return;
    if (empty())
    {
        *this = other;
        return;
    }

    const double w0 = static_cast<double>(filesLearned);
    const double w1 = static_cast<double>(juce::jmax(1, other.filesLearned));
    const double w = w0 + w1;

    tempo = (tempo * w0 + other.tempo * w1) / w;
    if (other.swing > 0.52)
        swing = swing > 0.52 ? (swing * w0 + other.swing * w1) / w : other.swing;
    laidBackMs = juce::jmax(laidBackMs, other.laidBackMs);
    if (other.defaultExtension > 0)
        defaultExtension = defaultExtension > 0
            ? ilerp(defaultExtension, other.defaultExtension, w1 / w)
            : other.defaultExtension;
    voicingSpread = (voicingSpread * w0 + other.voicingSpread * w1) / w;
    voicingMaxNotes = juce::jmax(voicingMaxNotes, other.voicingMaxNotes);
    voicingLow = juce::jmin(voicingLow, other.voicingLow);
    voicingHigh = juce::jmax(voicingHigh, other.voicingHigh);
    chromaticApproach = (chromaticApproach * w0 + other.chromaticApproach * w1) / w;
    swingSixteenths = swingSixteenths || other.swingSixteenths;
    rootless = rootless || other.rootless;
    if (w1 >= w0)
    {
        rootNote = other.rootNote;
        scale = other.scale;
        motifContour = other.motifContour;
        motifRhythm = other.motifRhythm;
    }

    mergePart(chords, other.chords, w0, w1);
    mergePart(bass, other.bass, w0, w1);
    mergePart(synth, other.synth, w0, w1);
    mergePart(piano, other.piano, w0, w1);
    mergePart(drums, other.drums, w0, w1);

    for (int i = 0; i < 13; ++i)
        melodyIntervals[static_cast<size_t>(i)] += other.melodyIntervals[static_cast<size_t>(i)];

    auto mergeList = [](std::vector<std::string>& into, const std::vector<std::string>& add)
    {
        for (const auto& item : add)
            addPattern(into, item);
    };
    mergeList(chordVocabulary, other.chordVocabulary);
    mergeList(bassPatterns, other.bassPatterns);
    mergeList(chordPatterns, other.chordPatterns);
    mergeList(pianoPatterns, other.pianoPatterns);

    for (const auto& from : other.chordTransitions)
        for (const auto& to : from.second)
            chordTransitions[from.first][to.first] += to.second;

    filesLearned += juce::jmax(1, other.filesLearned);
    sources.insert(sources.end(), other.sources.begin(), other.sources.end());
    finaliseSummary();
}

void StyleProfile::finaliseSummary()
{
    std::ostringstream out;
    out << (name.empty() ? "style" : name)
        << " · " << filesLearned << " file" << (filesLearned == 1 ? "" : "s")
        << " · " << juce::roundToInt(tempo) << " bpm · swing " << juce::String(swing, 2);
    if (swingSixteenths)
        out << " 16ths";
    out << " · ext " << defaultExtension;
    if (!chordVocabulary.empty())
    {
        out << "\nChords: ";
        for (size_t i = 0; i < juce::jmin(static_cast<size_t>(8), chordVocabulary.size()); ++i)
        {
            if (i) out << " ";
            out << chordVocabulary[i];
        }
    }
    if (chromaticApproach > 0.05)
        out << "\nBass approaches " << juce::roundToInt(chromaticApproach * 100.0) << "%";
    out << "\nVoicing spread " << juce::roundToInt(voicingSpread) << " st"
        << (rootless ? ", rootless" : "");
    summary = out.str();
}

StyleProfile StyleProfile::learnFromFiles(const juce::Array<juce::File>& files)
{
    StyleProfile combined;
    for (const auto& file : files)
    {
        if (file.isDirectory())
        {
            combined.absorb(learnFromFolder(file));
            continue;
        }
        if (!file.hasFileExtension("mid") && !file.hasFileExtension("midi"))
            continue;
        const auto analysis = MidiImporter::analyseFile(file);
        if (analysis.ok)
            combined.absorb(extract(analysis));
    }
    return combined;
}

StyleProfile StyleProfile::learnFromFolder(const juce::File& folder)
{
    juce::Array<juce::File> files;
    if (folder.isDirectory())
        folder.findChildFiles(files, juce::File::findFiles, false, "*.mid;*.midi");
    else
        files.add(folder);
    auto profile = learnFromFiles(files);
    if (profile.name.empty())
        profile.name = folder.isDirectory() ? folder.getFileNameWithoutExtension().toStdString()
                                            : folder.getParentDirectory().getFileNameWithoutExtension().toStdString();
    profile.finaliseSummary();
    return profile;
}

GenreProfile StyleProfile::blend(const GenreProfile& genre, const StyleProfile& style, double amount)
{
    const double t = juce::jlimit(0.0, 1.0, amount);
    if (t < 0.01 || style.empty())
        return genre;

    GenreProfile out = genre;
    if (style.swing > 0.51)
        out.swing = lerp(genre.swing, style.swing, t);
    if (t > 0.4)
        out.swingSixteenths = style.swingSixteenths || style.swing > 0.55;
    if (style.defaultExtension > 0)
        out.defaultExtension = ilerp(genre.defaultExtension, style.defaultExtension, t);
    out.anticipation = lerp(genre.anticipation, juce::jmax(genre.anticipation, 0.45), t);

    out.chordVoicing.low = ilerp(genre.chordVoicing.low, style.voicingLow, t);
    out.chordVoicing.high = ilerp(genre.chordVoicing.high, style.voicingHigh, t);
    out.chordVoicing.maxNotes = ilerp(genre.chordVoicing.maxNotes, style.voicingMaxNotes, t);
    if (t > 0.5)
        out.chordVoicing.rootless = style.rootless;
    out.pianoVoicing = out.chordVoicing;

    if (style.bass.registerHigh > style.bass.registerLow)
    {
        out.bassLow = ilerp(genre.bassLow, style.bass.registerLow, t);
        out.bassHigh = ilerp(genre.bassHigh, style.bass.registerHigh, t);
    }

    lerpMelody(out.leadMelody, genre.leadMelody, style.synth.density > 0.0 ? style.synth : style.piano,
               style, t);
    lerpMelody(out.pianoMelody, genre.pianoMelody, style.piano.density > 0.0 ? style.piano : style.synth,
               style, t);

    out.drums.laidBackMs = lerp(genre.drums.laidBackMs, style.laidBackMs, t);
    out.drums.hatSwing = lerp(genre.drums.hatSwing, style.swing, t);
    out.drums.humanizeMs = lerp(genre.drums.humanizeMs, juce::jmax(genre.drums.humanizeMs, 8.0), t);

    if (t > 0.4 && !style.bassPatterns.empty())
        out.bassPatterns = style.bassPatterns;
    if (t > 0.4 && style.chromaticApproach > 0.2)
    {
        addPattern(out.bassPatterns, "R--.....R-.T..A-");
        addPattern(out.bassPatterns, "R---..TR-...A.A-");
        addPattern(out.bassPatterns, "R-.A..R-R-.F..A-");
        out.slideChance = lerp(genre.slideChance, juce::jmax(genre.slideChance, 0.22), t);
    }
    if (t > 0.45 && !style.chordPatterns.empty())
        out.chordPatterns = style.chordPatterns;
    if (t > 0.45 && !style.pianoPatterns.empty())
        out.pianoPatterns = style.pianoPatterns;
    if (t > 0.6 && style.piano.density > 1.2)
        out.pianoStyle = PianoStyle::Comp;
    if (t > 0.55)
        out.leadMelody.useExtensions = style.defaultExtension >= 9;

    if (t > 0.5 && style.motifRhythm == "swung")
        out.swingSixteenths = true;

    return out;
}

GenreProfile StyleProfile::effectiveGenre(const MidiGenerationParams& params)
{
    const auto& genre = getGenreProfile(params.genre);
    if (params.styleProfileJson.empty() || params.styleBlend < 0.01)
        return genre;
    return blend(genre, fromJson(juce::String(params.styleProfileJson)), params.styleBlend);
}

std::vector<ChordInfo> StyleProfile::sampleProgression(const StyleProfile& style,
                                                       const MidiGenerationParams& params,
                                                       std::mt19937& rng)
{
    std::vector<ChordInfo> progression;
    if (style.chordVocabulary.empty())
        return progression;

    auto pickWeighted = [&rng](const std::map<std::string, double>& weights, const std::string& fallback)
    {
        double sum = 0.0;
        for (const auto& item : weights)
            sum += item.second;
        if (sum <= 0.0)
            return fallback;
        double cursor = std::uniform_real_distribution<double>(0.0, sum)(rng);
        for (const auto& item : weights)
        {
            cursor -= item.second;
            if (cursor <= 0.0)
                return item.first;
        }
        return weights.rbegin()->first;
    };

    std::map<std::string, double> startWeights;
    for (const auto& token : style.chordVocabulary)
        startWeights[token] = 1.0;
    for (const auto& from : style.chordTransitions)
        for (const auto& to : from.second)
            startWeights[from.first] += to.second;

    std::string token = pickWeighted(startWeights, style.chordVocabulary.front());
    const double beatsPerChord = 4.0;
    const double totalBeats = params.lengthInBars * 4.0;
    const int ext = style.defaultExtension;

    for (double beat = 0.0; beat < totalBeats - 1.0e-6; beat += beatsPerChord)
    {
        const auto spec = theory::parseChordToken(token);
        auto chord = theory::buildChord(params.rootNote, params.scale, spec, ext);
        chord.startBeat = beat;
        chord.lengthBeats = juce::jmin(beatsPerChord, totalBeats - beat);
        progression.push_back(chord);

        auto it = style.chordTransitions.find(token);
        if (it != style.chordTransitions.end() && !it->second.empty())
            token = pickWeighted(it->second, token);
        else
            token = style.chordVocabulary[rng() % style.chordVocabulary.size()];
    }
    return progression;
}

juce::String StyleProfile::promptHint() const
{
    juce::String hint;
    hint << "Learned style \"" << juce::String(name.empty() ? "user" : name) << "\": "
         << "swing " << juce::String(swing, 2)
         << ", extensions to " << defaultExtension
         << (rootless ? ", rootless voicings" : "")
         << (chromaticApproach > 0.15 ? ", chromatic approach bass" : "");
    if (!chordVocabulary.empty())
    {
        hint << ". Prefer these chord degrees:";
        for (size_t i = 0; i < juce::jmin(static_cast<size_t>(8), chordVocabulary.size()); ++i)
            hint << " " << juce::String(chordVocabulary[i]);
    }
    return hint;
}

juce::String StyleProfile::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", juce::String(name));
    juce::Array<juce::var> src;
    for (const auto& s : sources)
        src.add(juce::String(s));
    obj->setProperty("sources", src);
    obj->setProperty("filesLearned", filesLearned);
    obj->setProperty("tempo", tempo);
    obj->setProperty("swing", swing);
    obj->setProperty("swingSixteenths", swingSixteenths);
    obj->setProperty("laidBackMs", laidBackMs);
    obj->setProperty("defaultExtension", defaultExtension);
    obj->setProperty("rootNote", rootNote);
    obj->setProperty("scale", static_cast<int>(scale));
    juce::Array<juce::var> vocab;
    for (const auto& t : chordVocabulary)
        vocab.add(juce::String(t));
    obj->setProperty("chordVocabulary", vocab);

    juce::Array<juce::var> trans;
    for (const auto& from : chordTransitions)
        for (const auto& to : from.second)
        {
            auto* row = new juce::DynamicObject();
            row->setProperty("from", juce::String(from.first));
            row->setProperty("to", juce::String(to.first));
            row->setProperty("w", to.second);
            trans.add(juce::var(row));
        }
    obj->setProperty("chordTransitions", trans);
    obj->setProperty("voicingSpread", voicingSpread);
    obj->setProperty("voicingMaxNotes", voicingMaxNotes);
    obj->setProperty("rootless", rootless);
    obj->setProperty("voicingLow", voicingLow);
    obj->setProperty("voicingHigh", voicingHigh);
    obj->setProperty("chords", juce::var(partToObj(chords).get()));
    obj->setProperty("bass", juce::var(partToObj(bass).get()));
    obj->setProperty("synth", juce::var(partToObj(synth).get()));
    obj->setProperty("piano", juce::var(partToObj(piano).get()));
    obj->setProperty("drums", juce::var(partToObj(drums).get()));
    obj->setProperty("melodyIntervals", intervalsToVar(melodyIntervals));
    obj->setProperty("motifContour", juce::String(motifContour));
    obj->setProperty("motifRhythm", juce::String(motifRhythm));
    obj->setProperty("chromaticApproach", chromaticApproach);
    juce::Array<juce::var> bassP, chordP, pianoP;
    for (const auto& p : bassPatterns) bassP.add(juce::String(p));
    for (const auto& p : chordPatterns) chordP.add(juce::String(p));
    for (const auto& p : pianoPatterns) pianoP.add(juce::String(p));
    obj->setProperty("bassPatterns", bassP);
    obj->setProperty("chordPatterns", chordP);
    obj->setProperty("pianoPatterns", pianoP);
    obj->setProperty("summary", juce::String(summary));
    return juce::JSON::toString(juce::var(obj), false);
}

StyleProfile StyleProfile::fromJson(const juce::String& json)
{
    StyleProfile profile;
    auto parsed = juce::JSON::parse(json);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return profile;

    auto stringsFrom = [](const juce::var& value)
    {
        std::vector<std::string> out;
        if (auto* arr = value.getArray())
            for (const auto& item : *arr)
                out.push_back(item.toString().toStdString());
        return out;
    };

    profile.name = obj->getProperty("name").toString().toStdString();
    profile.sources = stringsFrom(obj->getProperty("sources"));
    profile.filesLearned = static_cast<int>(obj->getProperty("filesLearned"));
    profile.tempo = static_cast<double>(obj->getProperty("tempo"));
    profile.swing = static_cast<double>(obj->getProperty("swing"));
    profile.swingSixteenths = static_cast<bool>(obj->getProperty("swingSixteenths"));
    profile.laidBackMs = static_cast<double>(obj->getProperty("laidBackMs"));
    profile.defaultExtension = static_cast<int>(obj->getProperty("defaultExtension"));
    profile.rootNote = static_cast<int>(obj->getProperty("rootNote"));
    profile.scale = static_cast<ScaleType>(static_cast<int>(obj->getProperty("scale")));
    profile.chordVocabulary = stringsFrom(obj->getProperty("chordVocabulary"));

    if (auto* trans = obj->getProperty("chordTransitions").getArray())
    {
        for (const auto& item : *trans)
        {
            if (auto* row = item.getDynamicObject())
            {
                const auto from = row->getProperty("from").toString().toStdString();
                const auto to = row->getProperty("to").toString().toStdString();
                profile.chordTransitions[from][to] = static_cast<double>(row->getProperty("w"));
            }
        }
    }

    profile.voicingSpread = static_cast<double>(obj->getProperty("voicingSpread"));
    profile.voicingMaxNotes = static_cast<int>(obj->getProperty("voicingMaxNotes"));
    profile.rootless = static_cast<bool>(obj->getProperty("rootless"));
    profile.voicingLow = static_cast<int>(obj->getProperty("voicingLow"));
    profile.voicingHigh = static_cast<int>(obj->getProperty("voicingHigh"));
    profile.chords = partFromVar(obj->getProperty("chords"));
    profile.bass = partFromVar(obj->getProperty("bass"));
    profile.synth = partFromVar(obj->getProperty("synth"));
    profile.piano = partFromVar(obj->getProperty("piano"));
    profile.drums = partFromVar(obj->getProperty("drums"));
    varToGrid13(obj->getProperty("melodyIntervals"), profile.melodyIntervals);
    profile.motifContour = obj->getProperty("motifContour").toString().toStdString();
    profile.motifRhythm = obj->getProperty("motifRhythm").toString().toStdString();
    profile.chromaticApproach = static_cast<double>(obj->getProperty("chromaticApproach"));
    profile.bassPatterns = stringsFrom(obj->getProperty("bassPatterns"));
    profile.chordPatterns = stringsFrom(obj->getProperty("chordPatterns"));
    profile.pianoPatterns = stringsFrom(obj->getProperty("pianoPatterns"));
    profile.summary = obj->getProperty("summary").toString().toStdString();
    if (profile.filesLearned <= 0 && !profile.chordVocabulary.empty())
        profile.filesLearned = 1;
    if (profile.summary.empty())
        profile.finaliseSummary();
    return profile;
}

juce::File StyleProfile::storeDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("SynthesiaAI")
                   .getChildFile("styles");
    dir.createDirectory();
    return dir;
}

juce::String StyleProfile::sanitiseName(const juce::String& name)
{
    juce::String out;
    for (auto c : name)
        if (juce::CharacterFunctions::isLetterOrDigit(c) || c == '-' || c == '_' || c == ' ')
            out += c;
    out = out.trim().replaceCharacters(" ", "-");
    return out.isEmpty() ? "style" : out;
}

juce::StringArray StyleProfile::listSaved()
{
    juce::StringArray names;
    juce::Array<juce::File> files;
    storeDirectory().findChildFiles(files, juce::File::findFiles, false, "*.json");
    for (const auto& file : files)
        names.add(file.getFileNameWithoutExtension());
    names.sort(true);
    return names;
}

bool StyleProfile::saveNamed(const StyleProfile& profile)
{
    const auto name = sanitiseName(profile.name.empty() ? "style" : juce::String(profile.name));
    StyleProfile copy = profile;
    copy.name = name.toStdString();
    copy.finaliseSummary();
    const auto file = storeDirectory().getChildFile(name + ".json");
    return file.replaceWithText(copy.toJson());
}

StyleProfile StyleProfile::loadNamed(const juce::String& name)
{
    const auto file = storeDirectory().getChildFile(sanitiseName(name) + ".json");
    if (!file.existsAsFile())
        return {};
    return fromJson(file.loadFileAsString());
}

bool StyleProfile::writeNeoSoulFixtures(const juce::File& dir)
{
    dir.createDirectory();
    auto write = [&](const juce::String& name, const std::vector<RawNote>& notes, double tempo)
    {
        return MidiExporter::exportToFile(toBuffer(notes), dir.getChildFile(name), tempo);
    };

    const double tempo = 86.0;
    const double swing16 = 0.08; // laid-back even 16ths
    auto swung = [&](double beat)
    {
        const int step = juce::roundToInt(beat * 4.0);
        return (step % 2 == 1) ? beat + swing16 : beat;
    };
    auto swung8 = [&](double beat)
    {
        const int bar = static_cast<int>(std::floor(beat / 4.0));
        const int q = juce::jlimit(0, 3, juce::roundToInt(beat - bar * 4.0));
        static const double pos[4] = { 0.0, 0.64, 2.0, 2.64 };
        return bar * 4.0 + pos[q];
    };

    // Cm9 | Fm11 | Abmaj9 | G7b9  × 2
    const int chordCh = partChannel(Part::Chords);
    const std::vector<std::vector<int>> voicings{
        { 48, 51, 55, 58, 62 },       // Cm9  C Eb G Bb D
        { 41, 44, 48, 51, 55, 58 },   // Fm11 F Ab C Eb G Bb
        { 44, 48, 51, 55, 58 },       // Abmaj9 Ab C Eb G Bb
        { 43, 47, 50, 53, 56 }        // G7b9 G B D F Ab
    };
    std::vector<RawNote> chordNotes;
    for (int bar = 0; bar < 8; ++bar)
        for (int p : voicings[static_cast<size_t>(bar % 4)])
            chordNotes.push_back({ p, swung(bar * 4.0), 3.55, 74, chordCh });

    // Chromatic-approach bass, one bar each chord.
    const int bassCh = partChannel(Part::Bass);
    const int bassLines[4][4] = {
        { 36, 39, 43, 40 }, // C Eb G  E→F
        { 41, 44, 36, 43 }, // F Ab C  G→Ab
        { 44, 48, 51, 45 }, // Ab C Eb A→G
        { 43, 47, 38, 37 }  // G B  D  Db→C
    };
    std::vector<RawNote> bassNotes;
    for (int bar = 0; bar < 8; ++bar)
        for (int beat = 0; beat < 4; ++beat)
        {
            const int pitch = bassLines[bar % 4][beat];
            const double start = swung8(bar * 4.0 + beat);
            bassNotes.push_back({ pitch, start, beat == 3 ? 0.7 : 0.85, beat == 0 ? 96 : 78, bassCh });
        }

    // Piano: 16th comp with 9ths, swung.
    const int pianoCh = partChannel(Part::Piano);
    const int pianoHits[4][4] = {
        { 62, 63, 65, 62 }, // D Eb F D over Cm9
        { 65, 68, 70, 67 }, // F Ab Bb G over Fm11
        { 68, 70, 72, 70 }, // Ab Bb C Bb over Abmaj9
        { 67, 68, 70, 66 }  // G Ab Bb F# over G7
    };
    std::vector<RawNote> pianoNotes;
    for (int bar = 0; bar < 8; ++bar)
        for (int i = 0; i < 4; ++i)
        {
            const double start = swung(bar * 4.0 + i * 0.75);
            pianoNotes.push_back({ pianoHits[bar % 4][i], start, 0.45, 70 + (i == 0 ? 8 : 0), pianoCh });
            if (i % 2 == 0)
                pianoNotes.push_back({ pianoHits[bar % 4][i] - 12, start, 0.4, 64, pianoCh });
        }

    // Lead: neighbour-tone melody over the same changes.
    const int leadCh = partChannel(Part::Synth);
    const int lead[8][4] = {
        { 72, 74, 75, 72 },
        { 73, 72, 70, 68 },
        { 70, 72, 75, 77 },
        { 74, 75, 73, 71 },
        { 72, 75, 77, 79 },
        { 77, 75, 72, 70 },
        { 75, 77, 79, 80 },
        { 79, 77, 75, 74 }
    };
    std::vector<RawNote> leadNotes;
    for (int bar = 0; bar < 8; ++bar)
        for (int beat = 0; beat < 4; ++beat)
            leadNotes.push_back({ lead[bar][beat], swung8(bar * 4.0 + beat), 0.72, 84, leadCh });

    return write("neo-soul-chords-8bar.mid", chordNotes, tempo)
        && write("neo-soul-bass-8bar.mid", bassNotes, tempo)
        && write("neo-soul-keys-8bar.mid", pianoNotes, tempo)
        && write("neo-soul-lead-8bar.mid", leadNotes, tempo);
}
