// Offline "DAW" host: instantiates the plugin, generates a clip with the Markov
// generator, plays it through the plugin's synth with a running transport and
// bounces the result to WAV (plus the clip as .mid).

#include "PluginProcessor.h"
#include "MidiExporter.h"
#include "Generators/GenreProfiles.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

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

    void printUsage()
    {
        std::cout <<
            "Usage: SynthesiaRender --out clip.wav [options]\n"
            "  --generator <genre|markov>  arranger (default) or the classic Markov melody\n"
            "  --genre <name>          pop|lofi|house|trap|rnb|cinematic (default pop)\n"
            "  --part <name>           all|chords|bass|synth|piano|drums (default all)\n"
            "  --key <C..B>            root note (default C)\n"
            "  --scale <name>          major|minor|dorian|mixolydian|pentatonic (default major)\n"
            "  --bars <1-16>           clip length in bars (default 8)\n"
            "  --quant <1/16|1/8|1/4|1/2>  finest melodic grid (default: genre's)\n"
            "  --tempo <bpm>           host tempo (default: genre's)\n"
            "  --seed <n>              generator seed, 0 = random (default 1)\n"
            "  --style <text>          style prompt passed to the generator\n"
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

    const auto outPath = option("--out", {});
    if (outPath.isEmpty() || args.contains("--help"))
    {
        printUsage();
        return outPath.isEmpty() ? 1 : 0;
    }

    const int genreIndex = genreNames.indexOf(option("--genre", "pop"), true);
    const int partIndex = partNames.indexOf(option("--part", "all"), true);
    const bool useMarkov = option("--generator", "genre").equalsIgnoreCase("markov");

    if (genreIndex < 0 || partIndex < 0)
    {
        std::cerr << "Invalid --genre or --part\n";
        printUsage();
        return 1;
    }

    MidiGenerationParams params;
    params.genre = static_cast<Genre>(genreIndex);
    params.part = static_cast<Part>(partIndex);
    const auto& profile = getGenreProfile(params.genre);
    static const char* gridNames[] = { "1/16", "1/8", "1/4", "1/2" };

    params.rootNote = parseKey(option("--key", "C"));
    const int scaleIndex = parseScale(option("--scale", "major"));
    const int quantIndex = parseQuantization(option("--quant", gridNames[static_cast<int>(profile.defaultGrid)]));
    params.lengthInBars = juce::jlimit(1, 16, option("--bars", "8").getIntValue());
    params.tempo = juce::jlimit(20.0, 300.0, option("--tempo", juce::String(profile.defaultTempo)).getDoubleValue());
    params.seed = static_cast<unsigned int>(option("--seed", "1").getLargeIntValue());
    params.stylePrompt = option("--style", "").toStdString();

    if (params.rootNote < 0 || scaleIndex < 0 || quantIndex < 0)
    {
        std::cerr << "Invalid --key, --scale or --quant\n";
        printUsage();
        return 1;
    }

    params.scale = static_cast<ScaleType>(scaleIndex);
    params.quantization = static_cast<Quantization>(quantIndex);

    const double sampleRate = option("--sample-rate", "48000").getDoubleValue();
    const int blockSize = juce::jmax(16, option("--block", "512").getIntValue());

    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    auto* synthesia = dynamic_cast<SynthesiaAIProcessor*>(plugin.get());
    jassert(synthesia != nullptr);

    synthesia->setGeneratorType(useMarkov ? GeneratorType::Markov : GeneratorType::Genre);
    synthesia->generateMidiBlocking(params);

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

    OfflinePlayHead playHead;
    playHead.bpm = params.tempo;
    playHead.sampleRate = sampleRate;

    plugin->setPlayHead(&playHead);
    plugin->setPlayConfigDetails(0, 2, sampleRate, blockSize);
    plugin->setNonRealtime(true);
    plugin->prepareToPlay(sampleRate, blockSize);

    const double clipSeconds = synthesia->getClipLengthInBeats() * 60.0 / params.tempo;
    const auto clipSamples = static_cast<juce::int64>(clipSeconds * sampleRate);
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

    std::cout << "Rendered " << wavFile.getFullPathName() << "\n"
              << "  " << genreName(params.genre) << ", " << partName(params.part) << ", key "
              << option("--key", "C") << " " << option("--scale", "major") << "\n";
    if (chordChart.isNotEmpty())
        std::cout << "  chords: " << chordChart << "\n";
    std::cout << "  " << params.lengthInBars << " bars @ " << params.tempo << " bpm, "
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
