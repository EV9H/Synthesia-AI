// Offline "DAW" host: instantiates the plugin, generates a clip with the Markov
// generator, plays it through the plugin's synth with a running transport and
// bounces the result to WAV (plus the clip as .mid).

#include "PluginProcessor.h"
#include "MidiExporter.h"
#include "Generators/GenreProfiles.h"
#include "Generators/ArrangementPlan.h"
#include "MidiImporter.h"
#include "StyleProfile.h"
#include "PresetStore.h"
#include "Music/MusicTheory.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <optional>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
    class OfflinePlayHead : public juce::AudioPlayHead
    {
    public:
        double bpm = 120.0;
        double sampleRate = 48000.0;
        juce::int64 samplePosition = 0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            const double seconds = static_cast<double>(samplePosition) / sampleRate;
            info.setIsPlaying(true);
            info.setBpm(bpm);
            info.setTimeSignature(TimeSignature{ 4, 4 });
            info.setTimeInSamples(samplePosition);
            info.setTimeInSeconds(seconds);
            info.setPpqPosition(seconds * bpm / 60.0);
            return info;
        }
    };

    int parseKey(const juce::String& name)
    {
        static const juce::StringArray names{ "C", "C#", "D", "D#", "E", "F",
                                              "F#", "G", "G#", "A", "A#", "B" };
        return names.indexOf(name, true);
    }

    int parseScale(const juce::String& name)
    {
        static const juce::StringArray names{ "major", "minor", "dorian", "mixolydian", "pentatonic" };
        return names.indexOf(name.toLowerCase());
    }

    int parseQuantization(const juce::String& name)
    {
        static const juce::StringArray names{ "1/16", "1/8", "1/4", "1/2" };
        return names.indexOf(name);
    }

    const juce::StringArray genreNames{ "pop", "lofi", "house", "trap", "rnb", "cinematic" };
    const juce::StringArray partNames{ "all", "chords", "bass", "synth", "piano", "drums" };
    const juce::StringArray importModeNames{ "off", "accompany", "extend", "rewrite", "restyle" };

    void printUsage()
    {
        std::cout <<
            "Usage: SynthesiaRender --out clip.wav [options]\n"
            "  --generator <genre|markov|llm>  arranger (default), Markov, or LLM planner\n"
            "  --genre <name>          pop|lofi|house|trap|rnb|cinematic (default pop)\n"
            "  --part <name>           all|chords|bass|synth|piano|drums (default all)\n"
            "  --key <C..B>            root note (default C)\n"
            "  --scale <name>          major|minor|dorian|mixolydian|pentatonic (default major)\n"
            "  --bars <1-32>           clip length in bars (default 8)\n"
            "  --form <loop|song>      loop (default) or a sectioned song form\n"
            "  --chords <text>         locked progression, e.g. 'Am9 Fmaj9 Dm7 E7b9'\n"
            "  --lock-chords           keep the clip's / --chords progression on regen\n"
            "  --lock-parts <list>     comma-separated parts to keep (chords,bass,...)\n"
            "  --quant <1/16|1/8|1/4|1/2>  finest melodic grid (default: genre's)\n"
            "  --tempo <bpm>           host tempo (default: genre's)\n"
            "  --seed <n>              generator seed, 0 = random (default 1)\n"
            "  --style <text>          style prompt (LLM planner / keyword fallback)\n"
            "  --plan <file.json>      arrangement plan JSON (realised by the arranger)\n"
            "  --plan-json <json>      arrangement plan as a JSON string\n"
            "  --api-key <key>         OpenAI-compatible key (else OPENAI_API_KEY)\n"
            "  --api-url <url>         chat completions endpoint\n"
            "  --api-model <name>      model id (default gpt-4o-mini)\n"
            "  --import <file.mid>     analyse a MIDI file (key, chords, part roles)\n"
            "  --import-mode <name>    off|accompany|extend|rewrite|restyle (default off)\n"
            "  --write-fixtures <dir>  write demo piano-melody and chord-loop .mid files\n"
            "  --write-style-fixtures <dir>  write neo-soul style-learning .mid files\n"
            "  --learn-style <dir>     learn a style profile from a folder of .mid files\n"
            "  --save-style-json <file.json>  write the learned (or loaded) profile\n"
            "  --style-profile <file.json>    blend this learned profile with the genre\n"
            "  --style-blend <0-1>     0 = genre only, 1 = full learned style (default 1)\n"
            "  --preset <id>           factory/user preset (overrides genre/key/form/etc.)\n"
            "  --sidechain <on|off>    kick-bus ducking on pads/bass (default on)\n"
            "  --list-presets          print factory (and user) preset ids and exit\n"
            "  --midi <file.mid>       also export the clip (one track per part)\n"
            "  --split-midi            with --midi, also write <name>-<part>.mid files\n"
            "  --sample-rate <hz>      default 48000\n"
            "  --block <samples>       host block size (default 512)\n";
    }
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add(argv[i]);

    auto option = [&args](const juce::String& name, const juce::String& fallback)
    {
        const int index = args.indexOf(name);
        return (index >= 0 && index + 1 < args.size()) ? args[index + 1] : fallback;
    };

    if (args.contains("--list-presets"))
    {
        for (const auto& preset : PresetStore::factoryPresets())
            std::cout << preset.id << "\t" << preset.name << "\t" << preset.description << "\n";
        for (const auto& userId : PresetStore::listUserIds())
            std::cout << userId << "\t(user)\n";
        return 0;
    }

    const auto fixturesDir = option("--write-fixtures", {});
    if (fixturesDir.isNotEmpty())
    {
        const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile(fixturesDir);
        dir.createDirectory();
        const auto melody = dir.getChildFile("piano-melody-8bar.mid");
        const auto chords = dir.getChildFile("chord-loop-8bar.mid");
        if (!MidiImporter::writePianoMelody(melody) || !MidiImporter::writeChordLoop(chords))
        {
            std::cerr << "Failed to write fixture MIDI files\n";
            return 1;
        }
        std::cout << "Wrote " << melody.getFullPathName() << "\n"
                  << "Wrote " << chords.getFullPathName() << "\n";
    }

    const auto styleFixturesDir = option("--write-style-fixtures", {});
    if (styleFixturesDir.isNotEmpty())
    {
        const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile(styleFixturesDir);
        if (!StyleProfile::writeNeoSoulFixtures(dir))
        {
            std::cerr << "Failed to write style fixture MIDI files\n";
            return 1;
        }
        std::cout << "Wrote neo-soul style fixtures in " << dir.getFullPathName() << "\n";
    }

    StyleProfile learnedStyle;
    const auto learnDir = option("--learn-style", {});
    if (learnDir.isNotEmpty())
    {
        const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile(learnDir);
        learnedStyle = StyleProfile::learnFromFolder(dir);
        if (learnedStyle.empty())
        {
            std::cerr << "Learn-style found no usable MIDI in " << dir.getFullPathName() << "\n";
            return 1;
        }
        const auto saveName = option("--save-style-json", {});
        if (saveName.isNotEmpty())
            learnedStyle.name = juce::File(saveName).getFileNameWithoutExtension().toStdString();
        else if (learnedStyle.name.empty() || learnedStyle.name == "style")
            learnedStyle.name = "neo-soul";
        learnedStyle.finaliseSummary();
        std::cout << learnedStyle.summary << "\n";
    }

    const auto saveStylePath = option("--save-style-json", {});
    if (saveStylePath.isNotEmpty())
    {
        if (learnedStyle.empty())
        {
            const auto loadPath = option("--style-profile", {});
            if (loadPath.isNotEmpty())
                learnedStyle = StyleProfile::fromJson(
                    juce::File::getCurrentWorkingDirectory().getChildFile(loadPath).loadFileAsString());
        }
        if (learnedStyle.empty())
        {
            std::cerr << "No style profile to save\n";
            return 1;
        }
        const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile(saveStylePath);
        out.getParentDirectory().createDirectory();
        if (!out.replaceWithText(learnedStyle.toJson()))
        {
            std::cerr << "Failed to write " << out.getFullPathName() << "\n";
            return 1;
        }
        std::cout << "Wrote " << out.getFullPathName() << "\n";
        StyleProfile::saveNamed(learnedStyle);
    }

    const auto outPath = option("--out", {});
    if (outPath.isEmpty() || args.contains("--help"))
    {
        if ((fixturesDir.isNotEmpty() || styleFixturesDir.isNotEmpty() || learnDir.isNotEmpty()
             || saveStylePath.isNotEmpty()) && !args.contains("--help"))
            return 0;
        printUsage();
        return outPath.isEmpty() ? 1 : 0;
    }

    const auto presetId = option("--preset", {});
    std::optional<GeneratorPreset> loadedPreset;
    if (presetId.isNotEmpty())
    {
        loadedPreset = PresetStore::find(presetId);
        if (!loadedPreset)
        {
            std::cerr << "Unknown preset: " << presetId << "\n";
            return 1;
        }
    }

    const int genreIndex = genreNames.indexOf(option("--genre", "pop"), true);
    const int partIndex = partNames.indexOf(option("--part", "all"), true);
    const auto generatorName = option("--generator", loadedPreset ? PresetStore::generatorName(loadedPreset->generator)
                                                                  : "genre").toLowerCase();
    const bool useMarkov = generatorName == "markov";
    const bool useLlm = generatorName == "llm";

    if (genreIndex < 0 || partIndex < 0)
    {
        std::cerr << "Invalid --genre or --part\n";
        printUsage();
        return 1;
    }

    MidiGenerationParams params;
    if (loadedPreset)
        params = loadedPreset->params;
    params.genre = args.contains("--genre") || !loadedPreset ? static_cast<Genre>(genreIndex) : params.genre;
    params.part = args.contains("--part") || !loadedPreset ? static_cast<Part>(partIndex) : params.part;
    const auto& profile = getGenreProfile(params.genre);
    static const char* gridNames[] = { "1/16", "1/8", "1/4", "1/2" };

    const bool keyWasSet = args.contains("--key");
    const bool scaleWasSet = args.contains("--scale");
    const bool tempoWasSet = args.contains("--tempo");
    const bool barsWasSet = args.contains("--bars");

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* scaleNames[] = { "major", "minor", "dorian", "mixolydian", "pentatonic" };
    const juce::String keyFallback = loadedPreset ? juce::String(noteNames[juce::jlimit(0, 11, params.rootNote)]) : "C";
    const juce::String scaleFallback = loadedPreset ? juce::String(scaleNames[static_cast<int>(params.scale)]) : "major";
    const juce::String barsFallback = loadedPreset ? juce::String(params.lengthInBars) : "8";
    const juce::String tempoFallback = loadedPreset ? juce::String(params.tempo) : juce::String(profile.defaultTempo);
    const juce::String formFallback = loadedPreset && params.form == ArrangementForm::Song ? "song" : "loop";
    const juce::String styleFallback = loadedPreset ? juce::String(params.stylePrompt) : "";
    const juce::String chordsFallback = loadedPreset ? juce::String(params.chordText) : "";
    const juce::String seedFallback = loadedPreset ? juce::String(static_cast<int>(params.seed)) : "1";

    params.rootNote = parseKey(option("--key", keyFallback));
    const int scaleIndex = parseScale(option("--scale", scaleFallback));
    const int quantIndex = parseQuantization(option("--quant", gridNames[static_cast<int>(profile.defaultGrid)]));
    params.lengthInBars = juce::jlimit(1, 32, option("--bars", barsFallback).getIntValue());
    params.tempo = juce::jlimit(20.0, 300.0, option("--tempo", tempoFallback).getDoubleValue());
    params.seed = static_cast<unsigned int>(option("--seed", seedFallback).getLargeIntValue());
    params.stylePrompt = option("--style", styleFallback).toStdString();
    params.chordText = option("--chords", chordsFallback).toStdString();
    params.lockProgression = args.contains("--lock-chords") || params.chordText.size() > 0;
    params.form = option("--form", formFallback).equalsIgnoreCase("song") ? ArrangementForm::Song
                                                                         : ArrangementForm::Loop;

    const auto lockParts = option("--lock-parts", {});
    if (lockParts.isNotEmpty())
    {
        juce::StringArray names;
        names.addTokens(lockParts, ",", "");
        for (auto name : names)
        {
            const int index = partNames.indexOf(name.trim(), true);
            if (index > 0)
                params.lockedParts |= partLockBit(static_cast<Part>(index));
        }
    }

    if (params.rootNote < 0 || scaleIndex < 0 || quantIndex < 0)
    {
        std::cerr << "Invalid --key, --scale or --quant\n";
        printUsage();
        return 1;
    }

    params.scale = static_cast<ScaleType>(scaleIndex);
    params.quantization = static_cast<Quantization>(quantIndex);

    juce::String planJson = option("--plan-json", {});
    const auto planPath = option("--plan", {});
    if (planPath.isNotEmpty())
    {
        const juce::File planFile = juce::File::getCurrentWorkingDirectory().getChildFile(planPath);
        if (!planFile.existsAsFile())
        {
            std::cerr << "Plan file not found: " << planFile.getFullPathName() << "\n";
            return 1;
        }
        planJson = planFile.loadFileAsString();
    }
    params.planJson = planJson.toStdString();

    const auto styleProfilePath = option("--style-profile", {});
    if (styleProfilePath.isNotEmpty())
    {
        const juce::File styleFile = juce::File::getCurrentWorkingDirectory().getChildFile(styleProfilePath);
        if (!styleFile.existsAsFile())
        {
            std::cerr << "Style profile not found: " << styleFile.getFullPathName() << "\n";
            return 1;
        }
        const auto loaded = StyleProfile::fromJson(styleFile.loadFileAsString());
        if (loaded.empty())
        {
            std::cerr << "Style profile was empty: " << styleFile.getFullPathName() << "\n";
            return 1;
        }
        params.styleProfileJson = loaded.toJson().toStdString();
        params.styleProfileName = loaded.name;
        params.styleBlend = juce::jlimit(0.0, 1.0, option("--style-blend", "1").getDoubleValue());
        std::cout << loaded.summary << "\n  blend " << juce::roundToInt(params.styleBlend * 100.0) << "%\n";
    }
    else if (!learnedStyle.empty())
    {
        params.styleProfileJson = learnedStyle.toJson().toStdString();
        params.styleProfileName = learnedStyle.name;
        params.styleBlend = juce::jlimit(0.0, 1.0, option("--style-blend", "1").getDoubleValue());
    }
    else if (args.contains("--style-blend"))
    {
        params.styleBlend = juce::jlimit(0.0, 1.0, option("--style-blend", "0").getDoubleValue());
    }

    const double sampleRate = option("--sample-rate", "48000").getDoubleValue();
    const int blockSize = juce::jmax(16, option("--block", "512").getIntValue());

    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    auto* synthesia = dynamic_cast<SynthesiaAIProcessor*>(plugin.get());
    jassert(synthesia != nullptr);

    const auto apiKey = option("--api-key", juce::SystemStats::getEnvironmentVariable("OPENAI_API_KEY", {}));
    synthesia->setApiKey(apiKey);
    const auto apiUrl = option("--api-url", {});
    if (apiUrl.isNotEmpty())
        synthesia->setApiEndpoint(apiUrl);
    const auto apiModel = option("--api-model", {});
    if (apiModel.isNotEmpty())
        synthesia->setApiModel(apiModel);

    if (useMarkov)
        synthesia->setGeneratorType(GeneratorType::Markov);
    else if (useLlm)
        synthesia->setGeneratorType(GeneratorType::LLM);
    else
        synthesia->setGeneratorType(GeneratorType::Genre);

    const auto importPath = option("--import", {});
    const int importModeIndex = importModeNames.indexOf(option("--import-mode", "off"), true);
    if (importModeIndex < 0)
    {
        std::cerr << "Invalid --import-mode\n";
        return 1;
    }
    params.importMode = static_cast<ImportMode>(importModeIndex);

    if (importPath.isNotEmpty())
    {
        const juce::File importFile = juce::File::getCurrentWorkingDirectory().getChildFile(importPath);
        if (!synthesia->importMidiFile(importFile))
        {
            std::cerr << "Import failed: " << synthesia->getLastImport().error << "\n";
            return 1;
        }
        const auto& imported = synthesia->getLastImport();
        std::cout << imported.summary << "\n";
        if (!keyWasSet)
            params.rootNote = imported.rootNote;
        if (!scaleWasSet)
            params.scale = imported.scale;
        if (!tempoWasSet)
            params.tempo = imported.tempo;
        if (!barsWasSet && params.importMode != ImportMode::Extend)
            params.lengthInBars = imported.lengthInBars;
        if (params.chordText.empty())
        {
            params.chordText = imported.chordChart;
            params.lockProgression = true;
        }
        params.motifContour = imported.motifContour;
        params.motifRhythm = imported.motifRhythm;
    }

    const bool importOnly = importPath.isNotEmpty() && params.importMode == ImportMode::None;
    if (!importOnly)
        synthesia->generateMidiBlocking(params);

    // Import starts preview so the editor can audition a drop-in; the offline
    // host must not keep that looping clip running into the release tail.
    synthesia->setPreviewPlaying(false);

    const auto& clip = synthesia->getGeneratedMidi();
    if (clip.isEmpty())
    {
        std::cerr << "Generator produced no notes\n";
        return 2;
    }

    int notesInClip = 0;
    for (const auto metadata : clip)
        if (metadata.getMessage().isNoteOn())
            ++notesInClip;

    const auto midiPath = option("--midi", {});
    if (midiPath.isNotEmpty())
    {
        const juce::File midiFile = juce::File::getCurrentWorkingDirectory().getChildFile(midiPath);
        midiFile.getParentDirectory().createDirectory();
        if (!MidiExporter::exportToFile(clip, midiFile, params.tempo))
        {
            std::cerr << "Failed to write " << midiFile.getFullPathName() << "\n";
            return 3;
        }
        if (args.contains("--split-midi"))
            MidiExporter::exportSeparateParts(clip, midiFile, params.tempo);
    }

    juce::String chordChart;
    for (const auto& chord : synthesia->getProgression())
        chordChart << chord.symbol << " ";

    juce::String formChart;
    for (const auto& section : synthesia->getSections())
        formChart << section.label << " (" << juce::String(section.lengthBeats / 4.0, 0) << ") ";

    OfflinePlayHead playHead;
    playHead.bpm = synthesia->getClipTempo();
    playHead.sampleRate = sampleRate;

    plugin->setPlayHead(&playHead);
    plugin->setPlayConfigDetails(0, 2, sampleRate, blockSize);
    plugin->setNonRealtime(true);
    plugin->prepareToPlay(sampleRate, blockSize);

    const auto sidechainOpt = option("--sidechain", "on").toLowerCase();
    if (sidechainOpt == "off" || sidechainOpt == "0" || sidechainOpt == "false")
        synthesia->previewSynth().setSidechainAmount(0.0f);

    const double clipSeconds = synthesia->getClipLengthInBeats() * 60.0 / playHead.bpm;
    const auto clipSamples = juce::jmax(static_cast<juce::int64>(1),
        static_cast<juce::int64>(std::floor(clipSeconds * sampleRate)));
    const auto tailSamples = static_cast<juce::int64>((plugin->getTailLengthSeconds() + 0.2) * sampleRate);
    const auto totalSamples = clipSamples + tailSamples;

    juce::AudioBuffer<float> render(2, static_cast<int>(totalSamples));
    render.clear();

    juce::AudioBuffer<float> block(2, blockSize);
    juce::MidiBuffer midi;
    int notesOut = 0;

    for (juce::int64 pos = 0; pos < totalSamples;)
    {
        // Split the block at the clip end so the transport stops exactly there.
        const auto blockEnd = pos < clipSamples ? clipSamples : totalSamples;
        const int numSamples = static_cast<int>(std::min(static_cast<juce::int64>(blockSize), blockEnd - pos));
        block.setSize(2, numSamples, false, false, true);
        midi.clear();

        // Stop the transport after one pass of the clip so only the release tail rings out.
        if (pos >= clipSamples)
            plugin->setPlayHead(nullptr);

        playHead.samplePosition = pos;
        plugin->processBlock(block, midi);

        for (const auto metadata : midi)
            if (metadata.getMessage().isNoteOn())
                ++notesOut;

        for (int ch = 0; ch < 2; ++ch)
            render.copyFrom(ch, static_cast<int>(pos), block, ch, 0, numSamples);

        pos += numSamples;
    }

    plugin->releaseResources();

    const juce::File wavFile = juce::File::getCurrentWorkingDirectory().getChildFile(outPath);
    wavFile.getParentDirectory().createDirectory();
    wavFile.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(new juce::FileOutputStream(wavFile), sampleRate, 2, 24, {}, 0));

    if (writer == nullptr || !writer->writeFromAudioSampleBuffer(render, 0, render.getNumSamples()))
    {
        std::cerr << "Failed to write " << wavFile.getFullPathName() << "\n";
        return 4;
    }
    writer.reset();

    const float peak = render.getMagnitude(0, render.getNumSamples());
    const float rms = render.getRMSLevel(0, 0, render.getNumSamples());

    const auto& realisedGenre = synthesia->getLastPlan().sections.empty()
        ? params.genre : synthesia->getLastPlan().genre;
    const auto realisedKey = synthesia->getLastPlan().sections.empty()
        ? juce::String(theory::pitchClassName(params.rootNote,
                                              theory::keyPrefersFlats(params.rootNote, params.scale)))
        : juce::String(theory::pitchClassName(synthesia->getLastPlan().rootNote,
                                              theory::keyPrefersFlats(synthesia->getLastPlan().rootNote,
                                                                      synthesia->getLastPlan().scale)));
    std::cout << "Rendered " << wavFile.getFullPathName() << "\n"
              << "  " << genreName(realisedGenre) << ", " << partName(params.part) << ", key "
              << realisedKey;
    if (params.importMode != ImportMode::None)
        std::cout << ", " << importModeName(params.importMode);
    if (!params.styleProfileJson.empty())
        std::cout << ", style " << (params.styleProfileName.empty() ? "profile" : params.styleProfileName)
                  << " " << juce::roundToInt(params.styleBlend * 100.0) << "%";
    std::cout << "\n";
    if (chordChart.isNotEmpty())
        std::cout << "  chords: " << chordChart << "\n";
    if (formChart.isNotEmpty())
        std::cout << "  form: " << formChart << "\n";
    const auto& plan = synthesia->getLastPlan();
    if (!plan.sections.empty())
    {
        std::cout << "  plan (" << plan.source << "): " << plan.summary();
        if (plan.usage.totalTokens > 0)
            std::cout << "  tokens: " << plan.usage.promptTokens << " prompt / "
                      << plan.usage.completionTokens << " completion\n";
    }
    std::cout << "  " << synthesia->getClipLengthInBeats() / 4.0 << " bars @ " << playHead.bpm << " bpm, "
              << juce::String(totalSamples / sampleRate, 2) << " s, "
              << notesOut << "/" << notesInClip << " note-ons played\n"
              << "  peak " << juce::String(juce::Decibels::gainToDecibels(peak), 1) << " dBFS, "
              << "rms " << juce::String(juce::Decibels::gainToDecibels(rms), 1) << " dBFS\n";

    if (peak <= 0.0f || notesOut == 0)
    {
        std::cerr << "Render is silent\n";
        return 5;
    }
    if (peak >= 1.0f)
    {
        std::cerr << "Render clips\n";
        return 6;
    }
    if (notesOut != notesInClip)
    {
        std::cerr << "Plugin played " << notesOut << " notes but the clip has " << notesInClip << "\n";
        return 7;
    }

    return 0;
}
