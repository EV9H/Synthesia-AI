#include "PresetStore.h"

namespace
{
    GeneratorPreset makePreset(const juce::String& id, const juce::String& name,
                               const juce::String& description, Genre genre,
                               int root, ScaleType scale, int bars, double tempo,
                               ArrangementForm form, const juce::String& prompt,
                               const juce::String& chords = {}, unsigned int seed = 7)
    {
        GeneratorPreset preset;
        preset.id = id;
        preset.name = name;
        preset.description = description;
        preset.factory = true;
        preset.generator = GeneratorType::Genre;
        preset.params.genre = genre;
        preset.params.rootNote = root;
        preset.params.scale = scale;
        preset.params.lengthInBars = bars;
        preset.params.tempo = tempo;
        preset.params.form = form;
        preset.params.stylePrompt = prompt.toStdString();
        preset.params.chordText = chords.toStdString();
        preset.params.lockProgression = chords.isNotEmpty();
        preset.params.seed = seed;
        preset.params.part = Part::All;
        return preset;
    }

    const char* scaleName(ScaleType scale)
    {
        switch (scale)
        {
            case ScaleType::Major:      return "major";
            case ScaleType::Minor:      return "minor";
            case ScaleType::Dorian:     return "dorian";
            case ScaleType::Mixolydian: return "mixolydian";
            case ScaleType::Pentatonic: return "pentatonic";
        }
        return "major";
    }

    ScaleType scaleFromName(const juce::String& name)
    {
        const auto n = name.toLowerCase();
        if (n == "minor") return ScaleType::Minor;
        if (n == "dorian") return ScaleType::Dorian;
        if (n == "mixolydian") return ScaleType::Mixolydian;
        if (n == "pentatonic") return ScaleType::Pentatonic;
        return ScaleType::Major;
    }
}

juce::Array<GeneratorPreset> PresetStore::factoryPresets()
{
    juce::Array<GeneratorPreset> presets;
    presets.add(makePreset("pop-radio-c-major", "Pop Radio",
                           "Bright C-major radio pop, 8-bar loop",
                           Genre::Pop, 0, ScaleType::Major, 8, 110.0,
                           ArrangementForm::Loop,
                           "bright radio pop, catchy chorus, clean piano and clap groove"));
    presets.add(makePreset("lofi-am-dusty", "Lo-fi Dust",
                           "Dusty A-minor lo-fi / jazz loop",
                           Genre::LoFiJazz, 9, ScaleType::Minor, 8, 84.0,
                           ArrangementForm::Loop,
                           "dusty lo-fi beat, vinyl crackle feel, rhodes and lazy hats"));
    presets.add(makePreset("house-am-peak", "Peak Hour House",
                           "Peak-time A-minor house groove",
                           Genre::House, 9, ScaleType::Minor, 8, 124.0,
                           ArrangementForm::Loop,
                           "peak-time house, rolling bass, four-on-the-floor, bright stab chords"));
    presets.add(makePreset("trap-drop", "Trap Drop",
                           "A-minor trap drop with 808s",
                           Genre::Trap, 9, ScaleType::Minor, 8, 140.0,
                           ArrangementForm::Loop,
                           "trap drop, 808s, busy hats, dark synth stab"));
    presets.add(makePreset("rnb-neosoul", "Neo-Soul Pocket",
                           "D-minor neo-soul pocket",
                           Genre::RnB, 2, ScaleType::Minor, 8, 88.0,
                           ArrangementForm::Loop,
                           "neo-soul pocket, 9ths, lazy hats, warm electric piano"));
    presets.add(makePreset("cinematic-pad", "Cinematic Pads",
                           "C-minor cinematic pad bed",
                           Genre::Cinematic, 0, ScaleType::Minor, 8, 80.0,
                           ArrangementForm::Loop,
                           "wide cinematic pads, slow melody, sparse drums, film cue"));
    presets.add(makePreset("lofi-song-24", "Lo-fi Song",
                           "24-bar late-night lo-fi song form",
                           Genre::LoFiJazz, 9, ScaleType::Minor, 24, 84.0,
                           ArrangementForm::Song,
                           "late night lo-fi song, intro verse chorus, dusty keys",
                           {}, 11));
    presets.add(makePreset("house-song", "House Anthem",
                           "32-bar festival house anthem",
                           Genre::House, 9, ScaleType::Minor, 32, 124.0,
                           ArrangementForm::Song,
                           "festival house anthem, build and drop, big chorus",
                           {}, 11));
    presets.add(makePreset("locked-jazz-changes", "Jazz Changes",
                           "Locked ii-V-I jazz changes in C",
                           Genre::LoFiJazz, 0, ScaleType::Major, 8, 92.0,
                           ArrangementForm::Loop,
                           "bebop jazz changes, walking bass, comping piano",
                           "Dm7 G7 Cmaj7 Am7"));
    presets.add(makePreset("pop-uplift", "Pop Uplift",
                           "Uplifting G-major pop lift",
                           Genre::Pop, 7, ScaleType::Major, 8, 118.0,
                           ArrangementForm::Loop,
                           "uplifting pop, piano and chorus lift, anthemic"));
    return presets;
}

GeneratorPreset PresetStore::factoryByIndex(int index)
{
    const auto all = factoryPresets();
    if (index < 0 || index >= all.size())
        return all.getFirst();
    return all.getReference(index);
}

std::optional<GeneratorPreset> PresetStore::find(const juce::String& idOrName)
{
    const auto wanted = sanitiseId(idOrName);
    for (const auto& preset : factoryPresets())
        if (preset.id == wanted || preset.name == idOrName || sanitiseId(preset.name) == wanted)
            return preset;

    if (auto user = loadUser(wanted))
        return user;
    return std::nullopt;
}

juce::File PresetStore::userDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("SynthesiaAI")
                   .getChildFile("presets");
    dir.createDirectory();
    return dir;
}

juce::String PresetStore::sanitiseId(const juce::String& name)
{
    juce::String out;
    for (auto c : name.toLowerCase())
    {
        if (juce::CharacterFunctions::isLetterOrDigit(c))
            out += c;
        else if (c == ' ' || c == '-' || c == '_')
            out += '-';
    }
    while (out.contains("--"))
        out = out.replace("--", "-");
    return out.trimCharactersAtStart("-").trimCharactersAtEnd("-");
}

juce::StringArray PresetStore::listUserIds()
{
    juce::Array<juce::File> files;
    userDirectory().findChildFiles(files, juce::File::findFiles, false, "*.json");
    juce::StringArray ids;
    for (const auto& file : files)
        ids.add(file.getFileNameWithoutExtension());
    ids.sort(true);
    return ids;
}

juce::StringArray PresetStore::listAllIds()
{
    juce::StringArray ids;
    for (const auto& preset : factoryPresets())
        ids.add(preset.id);
    ids.addArray(listUserIds());
    return ids;
}

juce::String PresetStore::generatorName(GeneratorType type)
{
    switch (type)
    {
        case GeneratorType::Markov: return "markov";
        case GeneratorType::LLM:    return "llm";
        case GeneratorType::Genre:  return "genre";
    }
    return "genre";
}

GeneratorType PresetStore::generatorFromName(const juce::String& name)
{
    const auto n = name.toLowerCase();
    if (n == "markov") return GeneratorType::Markov;
    if (n == "llm") return GeneratorType::LLM;
    return GeneratorType::Genre;
}

void PresetStore::writeParams(juce::XmlElement& xml, const MidiGenerationParams& params)
{
    xml.setAttribute("rootNote", params.rootNote);
    xml.setAttribute("scale", static_cast<int>(params.scale));
    xml.setAttribute("lengthInBars", params.lengthInBars);
    xml.setAttribute("quantization", static_cast<int>(params.quantization));
    xml.setAttribute("tempo", params.tempo);
    xml.setAttribute("stylePrompt", juce::String(params.stylePrompt));
    xml.setAttribute("seed", static_cast<int>(params.seed));
    xml.setAttribute("genre", static_cast<int>(params.genre));
    xml.setAttribute("part", static_cast<int>(params.part));
    xml.setAttribute("form", static_cast<int>(params.form));
    xml.setAttribute("chordText", juce::String(params.chordText));
    xml.setAttribute("lockProgression", params.lockProgression ? 1 : 0);
    xml.setAttribute("lockedParts", params.lockedParts);
    xml.setAttribute("swing", params.swing);
    xml.setAttribute("motifContour", juce::String(params.motifContour));
    xml.setAttribute("motifRhythm", juce::String(params.motifRhythm));
    xml.setAttribute("enabledParts", params.enabledParts);
    xml.setAttribute("planJson", juce::String(params.planJson));
    xml.setAttribute("importMode", static_cast<int>(params.importMode));
    xml.setAttribute("styleProfileName", juce::String(params.styleProfileName));
    xml.setAttribute("styleBlend", params.styleBlend);
}

MidiGenerationParams PresetStore::readParams(const juce::XmlElement& xml)
{
    MidiGenerationParams params;
    params.rootNote = juce::jlimit(0, 11, xml.getIntAttribute("rootNote", 0));
    params.scale = static_cast<ScaleType>(juce::jlimit(0, 4, xml.getIntAttribute("scale", 0)));
    params.lengthInBars = juce::jlimit(1, 32, xml.getIntAttribute("lengthInBars", 8));
    params.quantization = static_cast<Quantization>(juce::jlimit(0, 3, xml.getIntAttribute("quantization", 1)));
    params.tempo = juce::jlimit(20.0, 300.0, xml.getDoubleAttribute("tempo", 120.0));
    params.stylePrompt = xml.getStringAttribute("stylePrompt").toStdString();
    params.seed = static_cast<unsigned int>(xml.getIntAttribute("seed", 0));
    params.genre = static_cast<Genre>(juce::jlimit(0, numGenres - 1, xml.getIntAttribute("genre", 0)));
    params.part = static_cast<Part>(juce::jlimit(0, 5, xml.getIntAttribute("part", 0)));
    params.form = xml.getIntAttribute("form", 0) == 1 ? ArrangementForm::Song : ArrangementForm::Loop;
    params.chordText = xml.getStringAttribute("chordText").toStdString();
    params.lockProgression = xml.getIntAttribute("lockProgression", 0) != 0;
    params.lockedParts = xml.getIntAttribute("lockedParts", 0);
    params.swing = xml.getDoubleAttribute("swing", 0.0);
    params.motifContour = xml.getStringAttribute("motifContour").toStdString();
    params.motifRhythm = xml.getStringAttribute("motifRhythm").toStdString();
    params.enabledParts = xml.getIntAttribute("enabledParts", 0);
    params.planJson = xml.getStringAttribute("planJson").toStdString();
    params.importMode = static_cast<ImportMode>(juce::jlimit(0, 4, xml.getIntAttribute("importMode", 0)));
    params.styleProfileName = xml.getStringAttribute("styleProfileName").toStdString();
    params.styleBlend = juce::jlimit(0.0, 1.0, xml.getDoubleAttribute("styleBlend", 0.0));
    return params;
}

juce::var PresetStore::paramsToVar(const MidiGenerationParams& params)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("rootNote", params.rootNote);
    obj->setProperty("scale", scaleName(params.scale));
    obj->setProperty("lengthInBars", params.lengthInBars);
    obj->setProperty("quantization", static_cast<int>(params.quantization));
    obj->setProperty("tempo", params.tempo);
    obj->setProperty("stylePrompt", juce::String(params.stylePrompt));
    obj->setProperty("seed", static_cast<int>(params.seed));
    obj->setProperty("genre", juce::String(genreName(params.genre)));
    obj->setProperty("part", juce::String(partName(params.part)));
    obj->setProperty("form", params.form == ArrangementForm::Song ? "song" : "loop");
    obj->setProperty("chordText", juce::String(params.chordText));
    obj->setProperty("lockProgression", params.lockProgression);
    obj->setProperty("lockedParts", params.lockedParts);
    obj->setProperty("swing", params.swing);
    obj->setProperty("motifContour", juce::String(params.motifContour));
    obj->setProperty("motifRhythm", juce::String(params.motifRhythm));
    obj->setProperty("enabledParts", params.enabledParts);
    obj->setProperty("importMode", juce::String(importModeName(params.importMode)));
    obj->setProperty("styleProfileName", juce::String(params.styleProfileName));
    obj->setProperty("styleBlend", params.styleBlend);
    return juce::var(obj);
}

MidiGenerationParams PresetStore::paramsFromVar(const juce::var& value)
{
    MidiGenerationParams params;
    if (auto* obj = value.getDynamicObject())
    {
        params.rootNote = juce::jlimit(0, 11, static_cast<int>(obj->getProperty("rootNote")));
        params.scale = scaleFromName(obj->getProperty("scale").toString());
        params.lengthInBars = juce::jlimit(1, 32, static_cast<int>(obj->getProperty("lengthInBars")));
        if (params.lengthInBars <= 0)
            params.lengthInBars = 8;
        params.quantization = static_cast<Quantization>(
            juce::jlimit(0, 3, static_cast<int>(obj->getProperty("quantization"))));
        params.tempo = juce::jlimit(20.0, 300.0, static_cast<double>(obj->getProperty("tempo")));
        if (params.tempo < 20.0)
            params.tempo = 120.0;
        params.stylePrompt = obj->getProperty("stylePrompt").toString().toStdString();
        params.seed = static_cast<unsigned int>(static_cast<int>(obj->getProperty("seed")));
        const auto genreStr = obj->getProperty("genre").toString();
        for (int i = 0; i < numGenres; ++i)
            if (genreStr.equalsIgnoreCase(genreName(static_cast<Genre>(i)))
                || genreStr.equalsIgnoreCase(juce::String(genreName(static_cast<Genre>(i)))
                                                 .replace(" / ", "-").replace(" ", "-")))
                params.genre = static_cast<Genre>(i);
        const auto partStr = obj->getProperty("part").toString();
        if (partStr.isNotEmpty())
        {
            if (partStr.equalsIgnoreCase("all") || partStr.equalsIgnoreCase("all parts"))
                params.part = Part::All;
            else
                for (auto part : allParts)
                    if (partStr.equalsIgnoreCase(partName(part)))
                        params.part = part;
        }
        params.form = obj->getProperty("form").toString().equalsIgnoreCase("song")
                          ? ArrangementForm::Song : ArrangementForm::Loop;
        params.chordText = obj->getProperty("chordText").toString().toStdString();
        params.lockProgression = static_cast<bool>(obj->getProperty("lockProgression"))
                                 || !params.chordText.empty();
        params.lockedParts = static_cast<int>(obj->getProperty("lockedParts"));
        params.swing = static_cast<double>(obj->getProperty("swing"));
        params.motifContour = obj->getProperty("motifContour").toString().toStdString();
        params.motifRhythm = obj->getProperty("motifRhythm").toString().toStdString();
        params.enabledParts = static_cast<int>(obj->getProperty("enabledParts"));
        const auto importStr = obj->getProperty("importMode").toString();
        if (importStr.equalsIgnoreCase("accompany")) params.importMode = ImportMode::Accompany;
        else if (importStr.equalsIgnoreCase("extend")) params.importMode = ImportMode::Extend;
        else if (importStr.equalsIgnoreCase("rewrite")) params.importMode = ImportMode::Rewrite;
        else if (importStr.equalsIgnoreCase("restyle")) params.importMode = ImportMode::Restyle;
        params.styleProfileName = obj->getProperty("styleProfileName").toString().toStdString();
        params.styleBlend = juce::jlimit(0.0, 1.0, static_cast<double>(obj->getProperty("styleBlend")));
    }
    return params;
}

bool PresetStore::saveUser(const juce::String& name, const MidiGenerationParams& params,
                           GeneratorType generator)
{
    const auto id = sanitiseId(name);
    if (id.isEmpty())
        return false;

    auto* obj = new juce::DynamicObject();
    obj->setProperty("id", id);
    obj->setProperty("name", name);
    obj->setProperty("factory", false);
    obj->setProperty("generator", generatorName(generator));
    obj->setProperty("params", paramsToVar(params));

    const auto file = userDirectory().getChildFile(id + ".json");
    return file.replaceWithText(juce::JSON::toString(juce::var(obj), true));
}

std::optional<GeneratorPreset> PresetStore::loadUser(const juce::String& id)
{
    const auto file = userDirectory().getChildFile(sanitiseId(id) + ".json");
    if (!file.existsAsFile())
        return std::nullopt;

    juce::var parsed;
    if (!juce::JSON::parse(file.loadFileAsString(), parsed).wasOk())
        return std::nullopt;
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    GeneratorPreset preset;
    preset.id = obj->getProperty("id").toString();
    if (preset.id.isEmpty())
        preset.id = sanitiseId(id);
    preset.name = obj->getProperty("name").toString();
    if (preset.name.isEmpty())
        preset.name = preset.id;
    preset.factory = false;
    preset.generator = generatorFromName(obj->getProperty("generator").toString());
    preset.params = paramsFromVar(obj->getProperty("params"));
    return preset;
}
