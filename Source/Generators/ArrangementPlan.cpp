#include "ArrangementPlan.h"
#include "../Music/MusicTheory.h"
#include "GenreProfiles.h"
#include <cctype>
#include <cmath>
#include <map>
#include <sstream>

namespace
{
    juce::String extractJsonObject(juce::String text)
    {
        text = text.trim();
        if (text.startsWith("```"))
        {
            const int firstNewline = text.indexOf("\n");
            const int lastFence = text.lastIndexOf("```");
            if (firstNewline > 0 && lastFence > firstNewline)
                text = text.substring(firstNewline + 1, lastFence).trim();
        }

        const int start = text.indexOf("{");
        const int end = text.lastIndexOf("}");
        if (start >= 0 && end > start)
            return text.substring(start, end + 1);
        return text;
    }

    juce::String lower(const juce::String& text)
    {
        return text.toLowerCase().trim();
    }

    double asNumber(const juce::var& value, double fallback)
    {
        if (value.isVoid() || value.isUndefined())
            return fallback;
        if (value.isString())
            return value.toString().getDoubleValue();
        return static_cast<double>(value);
    }

    bool asBool(const juce::var& value, bool fallback)
    {
        if (value.isVoid() || value.isUndefined())
            return fallback;
        if (value.isString())
        {
            const auto text = lower(value.toString());
            if (text == "true" || text == "1" || text == "yes" || text == "on")
                return true;
            if (text == "false" || text == "0" || text == "no" || text == "off")
                return false;
            return fallback;
        }
        return static_cast<bool>(value);
    }

    juce::String asString(const juce::var& value, const juce::String& fallback = {})
    {
        if (value.isVoid() || value.isUndefined())
            return fallback;
        return value.toString();
    }

    juce::var property(const juce::DynamicObject* obj, const char* name)
    {
        return obj != nullptr ? obj->getProperty(name) : juce::var();
    }

    juce::DynamicObject* asObject(const juce::var& value)
    {
        return value.getDynamicObject();
    }

    float clamp01(double value, float fallback)
    {
        if (!std::isfinite(value))
            return fallback;
        return juce::jlimit(0.0f, 1.0f, static_cast<float>(value));
    }

    std::string tidyContour(const juce::String& text)
    {
        const auto value = lower(text);
        if (value.contains("fall") || value.contains("descen"))
            return "falling";
        if (value.contains("ris") || value.contains("ascen"))
            return "rising";
        if (value.contains("stat") || value.contains("flat") || value.contains("pedal"))
            return "static";
        return "arch";
    }

    std::string tidyRhythm(const juce::String& text)
    {
        const auto value = lower(text);
        if (value.contains("swing") || value.contains("swung") || value.contains("shuffle"))
            return "swung";
        if (value.contains("sync"))
            return "syncopated";
        if (value.contains("sparse") || value.contains("minimal") || value.contains("space"))
            return "sparse";
        return "straight";
    }

    float energyForKind(SectionKind kind)
    {
        switch (kind)
        {
            case SectionKind::Intro:     return 0.32f;
            case SectionKind::Verse:     return 0.58f;
            case SectionKind::PreChorus: return 0.78f;
            case SectionKind::Chorus:    return 1.00f;
            case SectionKind::Drop:      return 1.00f;
            case SectionKind::Bridge:    return 0.52f;
            case SectionKind::Break:     return 0.28f;
            case SectionKind::Outro:     return 0.40f;
        }
        return 0.7f;
    }

    float defaultDensity(SectionKind kind, Part part)
    {
        const float energy = energyForKind(kind);
        switch (part)
        {
            case Part::Drums:  return kind == SectionKind::Break ? 0.22f : energy;
            case Part::Synth:  return kind == SectionKind::Intro ? 0.35f : energy;
            case Part::Piano:  return kind == SectionKind::Drop ? 0.45f : juce::jmin(1.0f, energy + 0.05f);
            case Part::Bass:   return juce::jmax(0.4f, energy);
            case Part::Chords:
            case Part::All:    return energy;
        }
        return energy;
    }

    std::vector<PlannedSection> defaultSections(Genre genre, int bars)
    {
        struct Spec { SectionKind kind; int bars; };
        const int safe = juce::jlimit(8, 32, bars);
        std::vector<Spec> spec;

        const bool dance = genre == Genre::House || genre == Genre::Trap;
        if (safe >= 28)
        {
            if (dance)
                spec = { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::Drop, 8 }, { SectionKind::Break, 4 },
                         { SectionKind::Drop, 4 }, { SectionKind::Outro, 4 } };
            else
                spec = { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::PreChorus, 4 }, { SectionKind::Chorus, 8 },
                         { SectionKind::Bridge, 4 }, { SectionKind::Outro, 4 } };
        }
        else
        {
            if (dance)
                spec = { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::Drop, 8 }, { SectionKind::Outro, 4 } };
            else
                spec = { { SectionKind::Intro, 4 }, { SectionKind::Verse, 8 },
                         { SectionKind::Chorus, 8 }, { SectionKind::Outro, 4 } };
        }

        std::vector<PlannedSection> sections;
        std::map<SectionKind, int> counts;
        int used = 0;
        for (const auto& item : spec)
        {
            if (used >= safe)
                break;
            PlannedSection section;
            section.kind = item.kind;
            section.bars = juce::jmin(item.bars, safe - used);
            section.energy = energyForKind(item.kind);
            if (++counts[item.kind] > 1)
                section.energy = juce::jmin(1.0f, section.energy + 0.06f);
            section.densityChords = defaultDensity(item.kind, Part::Chords);
            section.densityBass = defaultDensity(item.kind, Part::Bass);
            section.densitySynth = defaultDensity(item.kind, Part::Synth);
            section.densityPiano = defaultDensity(item.kind, Part::Piano);
            section.densityDrums = defaultDensity(item.kind, Part::Drums);
            sections.push_back(section);
            used += section.bars;
        }

        if (used < safe && !sections.empty())
            sections.back().bars += safe - used;
        return sections;
    }

    std::vector<std::string> chordTokens(const std::string& text)
    {
        std::vector<std::string> tokens;
        std::istringstream stream(text);
        for (std::string token; stream >> token;)
            if (token != "|")
                tokens.push_back(token);
        return tokens;
    }
}

Genre genreFromSlug(const juce::String& text)
{
    const auto value = lower(text);
    if (value.contains("lofi") || value.contains("lo-fi") || value.contains("jazz")
        || value.contains("chill") || value.contains("dusty"))
        return Genre::LoFiJazz;
    if (value.contains("trap") || value.contains("808") || value.contains("hip-hop")
        || value.contains("hiphop") || value.contains("hip hop"))
        return Genre::Trap;
    if (value.contains("rnb") || value.contains("r&b") || value.contains("neo-soul")
        || value.contains("neosoul") || value.contains("soul"))
        return Genre::RnB;
    if (value.contains("cinematic") || value.contains("orchestr") || value.contains("film")
        || value.contains("epic") || value.contains("score"))
        return Genre::Cinematic;
    if (value.contains("house") || value.contains("edm") || value.contains("synthwave")
        || value.contains("outrun") || value.contains("disco") || value.contains("techno")
        || value.contains("dance"))
        return Genre::House;
    if (value.contains("pop") || value.contains("radio"))
        return Genre::Pop;
    return Genre::Pop;
}

int keyFromName(const juce::String& text)
{
    static const juce::StringArray names{
        "c", "c#", "db", "d", "d#", "eb", "e", "f", "f#", "gb",
        "g", "g#", "ab", "a", "a#", "bb", "b"
    };
    static const int pcs[] = { 0, 1, 1, 2, 3, 3, 4, 5, 6, 6, 7, 8, 8, 9, 10, 10, 11 };
    auto token = lower(text).upToFirstOccurrenceOf(" ", false, false);
    token = token.removeCharacters("m");
    if (token.endsWith("major") || token.endsWith("minor"))
        token = token.dropLastCharacters(5);
    const int index = names.indexOf(token);
    return index >= 0 ? pcs[index] : -1;
}

ScaleType scaleFromName(const juce::String& text)
{
    const auto value = lower(text);
    if (value.contains("pent"))
        return ScaleType::Pentatonic;
    if (value.contains("mixo"))
        return ScaleType::Mixolydian;
    if (value.contains("dor"))
        return ScaleType::Dorian;
    if (value.contains("min") || value.contains("aeol"))
        return ScaleType::Minor;
    return ScaleType::Major;
}

SectionKind sectionFromName(const juce::String& text)
{
    const auto value = lower(text);
    if (value.contains("intro"))
        return SectionKind::Intro;
    if (value.contains("pre"))
        return SectionKind::PreChorus;
    if (value.contains("chorus") || value.contains("hook"))
        return SectionKind::Chorus;
    if (value.contains("bridge"))
        return SectionKind::Bridge;
    if (value.contains("drop"))
        return SectionKind::Drop;
    if (value.contains("break") || value.contains("breakd"))
        return SectionKind::Break;
    if (value.contains("outro") || value.contains("ending") || value.contains("coda"))
        return SectionKind::Outro;
    return SectionKind::Verse;
}

const char* ArrangementPlan::schemaPrompt()
{
    return
        "Return ONLY a compact JSON object (no markdown, no notes) with this schema:\n"
        "{\n"
        "  \"genre\": \"pop|lofi|house|trap|rnb|cinematic\",\n"
        "  \"key\": \"C|C#|Db|D|Eb|E|F|F#|G|Ab|A|Bb|B\",\n"
        "  \"scale\": \"major|minor|dorian|mixolydian|pentatonic\",\n"
        "  \"tempo\": 60-180,\n"
        "  \"swing\": 0.5-0.75,\n"
        "  \"bars\": 8-32,\n"
        "  \"parts\": {\"chords\":true,\"bass\":true,\"synth\":true,\"piano\":true,\"drums\":true},\n"
        "  \"motif\": {\"contour\":\"rising|falling|arch|static\",\"rhythm\":\"straight|swung|syncopated|sparse\"},\n"
        "  \"sections\": [\n"
        "    {\"name\":\"intro|verse|pre|chorus|bridge|drop|break|outro\",\n"
        "     \"bars\": 1-16,\n"
        "     \"energy\": 0-1,\n"
        "     \"chords\": \"Am9 Fmaj9\",\n"
        "     \"density\": {\"chords\":0-1,\"bass\":0-1,\"synth\":0-1,\"piano\":0-1,\"drums\":0-1}}\n"
        "  ]\n"
        "}\n"
        "Do not emit MIDI notes or pitches. Chord symbols only. Section bar counts should sum to bars.";
}

ArrangementPlan ArrangementPlan::fromKeywords(const std::string& prompt,
                                              const MidiGenerationParams& fallback)
{
    ArrangementPlan plan;
    plan.source = "heuristic";
    plan.prompt = prompt;
    plan.rootNote = fallback.rootNote;
    plan.scale = fallback.scale;
    plan.lengthInBars = fallback.lengthInBars < 16
        ? 24
        : juce::jlimit(8, 32, fallback.lengthInBars);
    plan.parts = { true, true, true, true, true };

    const juce::String text(prompt);
    const auto value = lower(text);

    plan.genre = value.isNotEmpty() ? genreFromSlug(text) : fallback.genre;
    if (value.isEmpty())
        plan.genre = fallback.genre;

    const auto& profile = getGenreProfile(plan.genre);
    plan.tempo = fallback.tempo > 0.0 ? fallback.tempo : profile.defaultTempo;
    plan.swing = profile.swing;
    plan.scale = fallback.scale;

    if (value.contains("minor") || value.contains("aeolian") || value.contains("sad")
        || value.contains("dark") || value.contains("night") || value.contains("noir"))
        plan.scale = ScaleType::Minor;
    else if (value.contains("dorian"))
        plan.scale = ScaleType::Dorian;
    else if (value.contains("mixo"))
        plan.scale = ScaleType::Mixolydian;
    else if (value.contains("major") || value.contains("bright") || value.contains("happy"))
        plan.scale = ScaleType::Major;

    const int key = keyFromName(text);
    if (key >= 0)
        plan.rootNote = key;

    if (value.contains("synthwave") || value.contains("outrun") || value.contains("late-night")
        || value.contains("late night") || value.contains("night"))
    {
        plan.genre = Genre::House;
        plan.tempo = juce::jmin(plan.tempo, 104.0);
        plan.scale = ScaleType::Minor;
        plan.motif.contour = "arch";
        plan.motif.rhythm = "straight";
    }
    if (value.contains("lofi") || value.contains("lo-fi") || value.contains("dusty"))
    {
        plan.genre = Genre::LoFiJazz;
        plan.swing = juce::jmax(plan.swing, 0.62);
        plan.motif.rhythm = "swung";
        plan.tempo = juce::jmin(plan.tempo, 90.0);
    }
    if (value.contains("trap") || value.contains("808"))
    {
        plan.genre = Genre::Trap;
        plan.tempo = juce::jmax(plan.tempo, 132.0);
        plan.motif.rhythm = "syncopated";
    }

    if (value.contains("swing") || value.contains("shuffle"))
    {
        plan.swing = juce::jmax(plan.swing, 0.62);
        plan.motif.rhythm = "swung";
    }
    if (value.contains("syncop"))
        plan.motif.rhythm = "syncopated";
    if (value.contains("sparse") || value.contains("minimal") || value.contains("intimate"))
        plan.motif.rhythm = "sparse";
    if (value.contains("ris"))
        plan.motif.contour = "rising";
    else if (value.contains("fall"))
        plan.motif.contour = "falling";
    else if (value.contains("static") || value.contains("pedal"))
        plan.motif.contour = "static";

    if (value.contains("no drum") || value.contains("without drum") || value.contains("undrum"))
        plan.parts[4] = false;
    if (value.contains("no piano"))
        plan.parts[3] = false;
    if (value.contains("no synth"))
        plan.parts[2] = false;

    if (value.contains("32") || value.contains("thirty-two"))
        plan.lengthInBars = 32;
    else if (value.contains("24") || value.contains("twenty-four"))
        plan.lengthInBars = 24;
    else if (value.contains("16"))
        plan.lengthInBars = 16;

    plan.sections = defaultSections(plan.genre, plan.lengthInBars);

    const bool explode = value.contains("explode") || value.contains("drop")
                      || value.contains("anthem") || value.contains("huge chorus")
                      || value.contains("chorus should");
    const bool sparse = plan.motif.rhythm == "sparse" || value.contains("sparse")
                     || value.contains("minimal");

    for (auto& section : plan.sections)
    {
        if (explode && (section.kind == SectionKind::Chorus || section.kind == SectionKind::Drop))
        {
            section.energy = 1.0f;
            section.densityDrums = 1.0f;
            section.densitySynth = 1.0f;
            section.densityBass = 1.0f;
        }
        if (explode && (section.kind == SectionKind::Intro || section.kind == SectionKind::Break))
        {
            section.energy = juce::jmin(section.energy, 0.24f);
            section.densityDrums = 0.28f;
            section.densitySynth = 0.22f;
        }
        if (sparse)
        {
            section.densityChords *= 0.7f;
            section.densityPiano *= 0.65f;
            section.densityDrums *= 0.55f;
            section.densitySynth *= 0.6f;
        }
    }

    validateAndRepair(plan, fallback);
    plan.source = "heuristic";
    return plan;
}

ArrangementPlan ArrangementPlan::parseJson(const juce::String& json,
                                           const MidiGenerationParams& fallback)
{
    const juce::String cleaned = extractJsonObject(json);
    juce::var parsed;
    if (cleaned.isEmpty() || !juce::JSON::parse(cleaned, parsed).wasOk() || asObject(parsed) == nullptr)
        return fromKeywords(fallback.stylePrompt, fallback);

    auto* obj = asObject(parsed);
    ArrangementPlan plan;
    plan.source = "json";
    plan.prompt = fallback.stylePrompt;
    plan.genre = fallback.genre;
    plan.rootNote = fallback.rootNote;
    plan.scale = fallback.scale;
    plan.tempo = fallback.tempo;
    plan.swing = 0.5;
    plan.lengthInBars = fallback.lengthInBars;
    plan.parts = { true, true, true, true, true };

    const auto genreText = asString(property(obj, "genre"));
    if (genreText.isNotEmpty())
        plan.genre = genreFromSlug(genreText);

    const int key = keyFromName(asString(property(obj, "key")));
    if (key >= 0)
        plan.rootNote = key;

    const auto scaleText = asString(property(obj, "scale"));
    if (scaleText.isNotEmpty())
        plan.scale = scaleFromName(scaleText);

    plan.tempo = asNumber(property(obj, "tempo"), plan.tempo);
    plan.swing = asNumber(property(obj, "swing"), plan.swing);
    plan.lengthInBars = juce::roundToInt(asNumber(property(obj, "bars"), plan.lengthInBars));

    if (auto* parts = asObject(property(obj, "parts")))
    {
        plan.parts[0] = asBool(parts->getProperty("chords"), true);
        plan.parts[1] = asBool(parts->getProperty("bass"), true);
        plan.parts[2] = asBool(parts->getProperty("synth"), true);
        plan.parts[3] = asBool(parts->getProperty("piano"), true);
        plan.parts[4] = asBool(parts->getProperty("drums"), true);
    }

    if (auto* motif = asObject(property(obj, "motif")))
    {
        plan.motif.contour = tidyContour(asString(motif->getProperty("contour"), "arch"));
        plan.motif.rhythm = tidyRhythm(asString(motif->getProperty("rhythm"), "straight"));
    }

    if (auto* sections = property(obj, "sections").getArray())
    {
        for (const auto& item : *sections)
        {
            auto* sectionObj = asObject(item);
            if (sectionObj == nullptr)
                continue;

            PlannedSection section;
            section.kind = sectionFromName(asString(sectionObj->getProperty("name")));
            if (sectionObj->hasProperty("kind"))
                section.kind = sectionFromName(asString(sectionObj->getProperty("kind")));
            section.bars = juce::roundToInt(asNumber(sectionObj->getProperty("bars"), 4.0));
            section.energy = clamp01(asNumber(sectionObj->getProperty("energy"), energyForKind(section.kind)),
                                     energyForKind(section.kind));
            section.chords = asString(sectionObj->getProperty("chords")).toStdString();
            section.densityChords = defaultDensity(section.kind, Part::Chords);
            section.densityBass = defaultDensity(section.kind, Part::Bass);
            section.densitySynth = defaultDensity(section.kind, Part::Synth);
            section.densityPiano = defaultDensity(section.kind, Part::Piano);
            section.densityDrums = defaultDensity(section.kind, Part::Drums);

            if (auto* density = asObject(sectionObj->getProperty("density")))
            {
                section.densityChords = clamp01(asNumber(density->getProperty("chords"), section.densityChords),
                                                section.densityChords);
                section.densityBass = clamp01(asNumber(density->getProperty("bass"), section.densityBass),
                                              section.densityBass);
                section.densitySynth = clamp01(asNumber(density->getProperty("synth"), section.densitySynth),
                                               section.densitySynth);
                section.densityPiano = clamp01(asNumber(density->getProperty("piano"), section.densityPiano),
                                               section.densityPiano);
                section.densityDrums = clamp01(asNumber(density->getProperty("drums"), section.densityDrums),
                                               section.densityDrums);
            }

            plan.sections.push_back(section);
        }
    }

    validateAndRepair(plan, fallback);
    plan.source = "json";
    return plan;
}

void ArrangementPlan::validateAndRepair(ArrangementPlan& plan, const MidiGenerationParams& fallback)
{
    if (static_cast<int>(plan.genre) < 0 || static_cast<int>(plan.genre) >= numGenres)
        plan.genre = fallback.genre;

    plan.rootNote = juce::jlimit(0, 11, plan.rootNote);
    plan.tempo = juce::jlimit(60.0, 180.0, std::isfinite(plan.tempo) ? plan.tempo : 120.0);
    plan.swing = juce::jlimit(0.5, 0.75, std::isfinite(plan.swing) ? plan.swing : 0.5);
    plan.lengthInBars = juce::jlimit(1, 32, plan.lengthInBars);

    plan.motif.contour = tidyContour(plan.motif.contour);
    plan.motif.rhythm = tidyRhythm(plan.motif.rhythm);

    bool anyPart = false;
    for (bool enabled : plan.parts)
        anyPart = anyPart || enabled;
    if (!anyPart)
        plan.parts = { true, true, true, true, true };

    if (plan.sections.empty())
        plan.sections = defaultSections(plan.genre, plan.lengthInBars < 8 ? 24 : plan.lengthInBars);

    int totalBars = 0;
    std::map<SectionKind, int> counts;
    std::string previousChords;
    for (auto& section : plan.sections)
    {
        section.bars = juce::jlimit(1, 16, section.bars);
        section.energy = clamp01(section.energy, energyForKind(section.kind));
        section.densityChords = clamp01(section.densityChords, defaultDensity(section.kind, Part::Chords));
        section.densityBass = clamp01(section.densityBass, defaultDensity(section.kind, Part::Bass));
        section.densitySynth = clamp01(section.densitySynth, defaultDensity(section.kind, Part::Synth));
        section.densityPiano = clamp01(section.densityPiano, defaultDensity(section.kind, Part::Piano));
        section.densityDrums = clamp01(section.densityDrums, defaultDensity(section.kind, Part::Drums));
        ++counts[section.kind];

        if (section.chords.empty())
            section.chords = previousChords;
        else
            previousChords = section.chords;

        totalBars += section.bars;
    }

    if (totalBars <= 0)
    {
        plan.sections = defaultSections(plan.genre, 24);
        totalBars = 24;
    }

    if (totalBars > 32)
    {
        int keep = 0;
        for (auto it = plan.sections.begin(); it != plan.sections.end();)
        {
            if (keep >= 32)
            {
                it = plan.sections.erase(it);
                continue;
            }
            it->bars = juce::jmin(it->bars, 32 - keep);
            keep += it->bars;
            ++it;
        }
        totalBars = keep;
    }

    plan.lengthInBars = totalBars;

    const auto& profile = getGenreProfile(plan.genre);
    const bool preferFlats = theory::keyPrefersFlats(plan.rootNote, plan.scale);

    for (auto& section : plan.sections)
    {
        if (!section.chords.empty())
            continue;

        const auto& templates = theory::isMinorMode(plan.scale) ? profile.minorProgressions
                                                                : profile.majorProgressions;
        if (templates.empty())
            continue;

        const auto& chosen = templates.front();
        std::vector<std::string> tokens;
        std::istringstream stream(chosen.tokens);
        for (std::string token; stream >> token;)
            tokens.push_back(token);

        std::string symbols;
        for (int i = 0; i < section.bars; ++i)
        {
            const auto spec = theory::parseChordToken(tokens[static_cast<size_t>(i) % tokens.size()]);
            auto chord = theory::buildChord(plan.rootNote, plan.scale, spec, profile.defaultExtension);
            if (!symbols.empty())
                symbols += " ";
            symbols += chord.symbol.empty()
                ? theory::chordSymbol(chord.rootPitchClass, chord.intervals, preferFlats)
                : chord.symbol;
        }
        section.chords = symbols;
    }
}

juce::String ArrangementPlan::toJson() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("genre", juce::String(genreSlug(genre)));
    obj->setProperty("key", juce::String(theory::pitchClassName(rootNote, theory::keyPrefersFlats(rootNote, scale))));
    static const char* scaleNames[] = { "major", "minor", "dorian", "mixolydian", "pentatonic" };
    obj->setProperty("scale", juce::String(scaleNames[static_cast<int>(scale)]));
    obj->setProperty("tempo", tempo);
    obj->setProperty("swing", swing);
    obj->setProperty("bars", lengthInBars);

    auto* partsObj = new juce::DynamicObject();
    partsObj->setProperty("chords", parts[0]);
    partsObj->setProperty("bass", parts[1]);
    partsObj->setProperty("synth", parts[2]);
    partsObj->setProperty("piano", parts[3]);
    partsObj->setProperty("drums", parts[4]);
    obj->setProperty("parts", juce::var(partsObj));

    auto* motifObj = new juce::DynamicObject();
    motifObj->setProperty("contour", juce::String(motif.contour));
    motifObj->setProperty("rhythm", juce::String(motif.rhythm));
    obj->setProperty("motif", juce::var(motifObj));

    juce::Array<juce::var> sectionList;
    for (const auto& section : sections)
    {
        auto* sectionObj = new juce::DynamicObject();
        sectionObj->setProperty("name", juce::String(sectionName(section.kind)).toLowerCase());
        sectionObj->setProperty("bars", section.bars);
        sectionObj->setProperty("energy", section.energy);
        sectionObj->setProperty("chords", juce::String(section.chords));
        auto* density = new juce::DynamicObject();
        density->setProperty("chords", section.densityChords);
        density->setProperty("bass", section.densityBass);
        density->setProperty("synth", section.densitySynth);
        density->setProperty("piano", section.densityPiano);
        density->setProperty("drums", section.densityDrums);
        sectionObj->setProperty("density", juce::var(density));
        sectionList.add(juce::var(sectionObj));
    }
    obj->setProperty("sections", sectionList);

    return juce::JSON::toString(juce::var(obj), false);
}

std::string ArrangementPlan::chordChart() const
{
    std::string out;
    for (const auto& section : sections)
    {
        const auto tokens = chordTokens(section.chords);
        for (int bar = 0; bar < section.bars; ++bar)
        {
            if (!out.empty())
                out += " | ";
            if (tokens.empty())
                out += "-";
            else if (section.chords.find('|') != std::string::npos)
            {
                std::vector<std::string> bars;
                std::istringstream stream(section.chords);
                for (std::string cell; std::getline(stream, cell, '|');)
                {
                    while (!cell.empty() && std::isspace(static_cast<unsigned char>(cell.front())))
                        cell.erase(cell.begin());
                    while (!cell.empty() && std::isspace(static_cast<unsigned char>(cell.back())))
                        cell.pop_back();
                    if (!cell.empty())
                        bars.push_back(cell);
                }
                out += bars.empty() ? "-" : bars[static_cast<size_t>(bar) % bars.size()];
            }
            else
            {
                out += tokens[static_cast<size_t>(bar) % tokens.size()];
            }
        }
    }
    return out;
}

std::string ArrangementPlan::summary() const
{
    std::ostringstream stream;
    static const char* scaleNames[] = { "major", "minor", "dorian", "mixolydian", "pentatonic" };
    stream << source << " · " << genreName(genre) << " · "
           << theory::pitchClassName(rootNote, theory::keyPrefersFlats(rootNote, scale))
           << " " << scaleNames[static_cast<int>(scale)]
           << " · " << juce::roundToInt(tempo) << " bpm · " << lengthInBars << " bars";
    if (motif.contour.size() + motif.rhythm.size() > 0)
        stream << " · motif " << motif.contour << "/" << motif.rhythm;
    stream << "\n";
    for (const auto& section : sections)
    {
        stream << sectionName(section.kind) << " " << section.bars
               << "  " << (section.chords.empty() ? "-" : section.chords)
               << "  e=" << juce::String(section.energy, 2) << "\n";
    }
    return stream.str();
}

std::vector<SectionInfo> ArrangementPlan::toSectionInfo() const
{
    std::vector<SectionInfo> result;
    std::map<SectionKind, int> counts;
    double beat = 0.0;
    for (const auto& section : sections)
    {
        const int occurrence = ++counts[section.kind];
        std::string label = sectionName(section.kind);
        if (occurrence > 1)
            label += " " + std::to_string(occurrence);
        SectionInfo info;
        info.kind = section.kind;
        info.startBeat = beat;
        info.lengthBeats = section.bars * 4.0;
        info.energy = section.energy;
        info.occurrence = occurrence;
        info.label = label;
        info.densityChords = section.densityChords;
        info.densityBass = section.densityBass;
        info.densitySynth = section.densitySynth;
        info.densityPiano = section.densityPiano;
        info.densityDrums = section.densityDrums;
        result.push_back(info);
        beat += info.lengthBeats;
    }
    return result;
}

std::vector<ChordInfo> ArrangementPlan::toProgression() const
{
    const auto& profile = getGenreProfile(genre);
    std::vector<ChordInfo> progression;
    double beat = 0.0;
    for (const auto& section : sections)
    {
        auto parsed = theory::parseProgressionText(section.chords, rootNote, scale,
                                                   section.bars, profile.defaultExtension);
        for (auto& chord : parsed)
        {
            chord.startBeat += beat;
            progression.push_back(std::move(chord));
        }
        beat += section.bars * 4.0;
    }
    return progression;
}

void ArrangementPlan::applyTo(MidiGenerationParams& params) const
{
    params.genre = genre;
    params.rootNote = rootNote;
    params.scale = scale;
    params.tempo = tempo;
    params.swing = swing;
    params.lengthInBars = lengthInBars;
    params.form = sections.size() > 1 ? ArrangementForm::Song : ArrangementForm::Loop;
    params.motifContour = motif.contour;
    params.motifRhythm = motif.rhythm;
    const int mask = enabledMask();
    const int all = partLockBit(Part::Chords) | partLockBit(Part::Bass) | partLockBit(Part::Synth)
                  | partLockBit(Part::Piano) | partLockBit(Part::Drums);
    params.enabledParts = mask == all ? 0 : mask;
    params.plannedSections = toSectionInfo();
    params.chordText = chordChart();
    auto progression = toProgression();
    if (!progression.empty())
        params.progression = std::move(progression);
}

bool ArrangementPlan::partEnabled(Part part) const
{
    switch (part)
    {
        case Part::Chords: return parts[0];
        case Part::Bass:   return parts[1];
        case Part::Synth:  return parts[2];
        case Part::Piano:  return parts[3];
        case Part::Drums:  return parts[4];
        case Part::All:    return true;
    }
    return true;
}

int ArrangementPlan::enabledMask() const
{
    int mask = 0;
    if (parts[0]) mask |= partLockBit(Part::Chords);
    if (parts[1]) mask |= partLockBit(Part::Bass);
    if (parts[2]) mask |= partLockBit(Part::Synth);
    if (parts[3]) mask |= partLockBit(Part::Piano);
    if (parts[4]) mask |= partLockBit(Part::Drums);
    return mask;
}
