#include "InstrumentLibrary.h"
#include "DrumBinaryData.h"
#include <cmath>
#include <memory>

InstrumentLibrary& InstrumentLibrary::get()
{
    static InstrumentLibrary library;
    return library;
}

void InstrumentLibrary::ensureLoaded()
{
    if (!loaded)
        load();
}

const InstrumentLibrary::Sample* InstrumentLibrary::find(SampleKind kind, int midiNote, float velocity) const
{
    if (kind == SampleKind::Off)
        return nullptr;

    const auto& list = byKind[static_cast<int>(kind)];
    if (list.empty())
        return nullptr;

    const bool wantHard = velocity >= 0.55f;
    const Sample* best = nullptr;
    int bestScore = 10000;
    for (const Sample* s : list)
    {
        int score = std::abs(s->rootNote - midiNote) * 4;
        if (s->hard != wantHard)
            score += 3;
        if (score < bestScore)
        {
            bestScore = score;
            best = s;
        }
    }
    return best;
}

void InstrumentLibrary::loadOne(const char* key, SampleKind kind, int rootNote, bool hard, bool loopable,
                                const char* data, int size)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::InputStream> stream(
        new juce::MemoryInputStream(data, static_cast<size_t>(size), false));
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(stream)));
    if (reader == nullptr)
        return;

    Sample sample;
    sample.sourceRate = reader->sampleRate;
    sample.rootNote = rootNote;
    sample.hard = hard;
    sample.loopable = loopable;
    const int length = static_cast<int>(reader->lengthInSamples);
    const int chans = juce::jlimit(1, 2, (int) reader->numChannels);
    sample.data.setSize(chans, juce::jmax(1, length));
    sample.data.clear();
    reader->read(&sample.data, 0, length, 0, true, true);
    if (loopable && length > 64)
    {
        sample.loopStart = juce::jmax(16, (int) (length * 0.22));
        sample.loopEnd = juce::jmax(sample.loopStart + 32, (int) (length * 0.78));
    }
    else
    {
        sample.loopEnd = length;
    }

    auto [it, inserted] = table.emplace(key, std::move(sample));
    if (inserted)
        byKind[static_cast<int>(kind)].push_back(&it->second);
}

void InstrumentLibrary::load()
{
    loaded = true;

#define LOAD(name, kind, note, hard, loop) \
    loadOne(#name, kind, note, hard, loop, DrumBinaryData::name##_wav, DrumBinaryData::name##_wavSize)

    LOAD(inst_piano_36_soft, SampleKind::Piano, 36, false, false);
    LOAD(inst_piano_36_hard, SampleKind::Piano, 36, true,  false);
    LOAD(inst_piano_42_soft, SampleKind::Piano, 42, false, false);
    LOAD(inst_piano_42_hard, SampleKind::Piano, 42, true,  false);
    LOAD(inst_piano_48_soft, SampleKind::Piano, 48, false, false);
    LOAD(inst_piano_48_hard, SampleKind::Piano, 48, true,  false);
    LOAD(inst_piano_54_soft, SampleKind::Piano, 54, false, false);
    LOAD(inst_piano_54_hard, SampleKind::Piano, 54, true,  false);
    LOAD(inst_piano_60_soft, SampleKind::Piano, 60, false, false);
    LOAD(inst_piano_60_hard, SampleKind::Piano, 60, true,  false);
    LOAD(inst_piano_66_soft, SampleKind::Piano, 66, false, false);
    LOAD(inst_piano_66_hard, SampleKind::Piano, 66, true,  false);
    LOAD(inst_piano_72_soft, SampleKind::Piano, 72, false, false);
    LOAD(inst_piano_72_hard, SampleKind::Piano, 72, true,  false);
    LOAD(inst_piano_78_soft, SampleKind::Piano, 78, false, false);
    LOAD(inst_piano_78_hard, SampleKind::Piano, 78, true,  false);
    LOAD(inst_piano_84_soft, SampleKind::Piano, 84, false, false);
    LOAD(inst_piano_84_hard, SampleKind::Piano, 84, true,  false);

    LOAD(inst_ep_36_soft, SampleKind::Ep, 36, false, false);
    LOAD(inst_ep_36_hard, SampleKind::Ep, 36, true,  false);
    LOAD(inst_ep_48_soft, SampleKind::Ep, 48, false, false);
    LOAD(inst_ep_48_hard, SampleKind::Ep, 48, true,  false);
    LOAD(inst_ep_54_soft, SampleKind::Ep, 54, false, false);
    LOAD(inst_ep_54_hard, SampleKind::Ep, 54, true,  false);
    LOAD(inst_ep_60_soft, SampleKind::Ep, 60, false, false);
    LOAD(inst_ep_60_hard, SampleKind::Ep, 60, true,  false);
    LOAD(inst_ep_72_soft, SampleKind::Ep, 72, false, false);
    LOAD(inst_ep_72_hard, SampleKind::Ep, 72, true,  false);

    LOAD(inst_str_36, SampleKind::Strings, 36, true, true);
    LOAD(inst_str_48, SampleKind::Strings, 48, true, true);
    LOAD(inst_str_60, SampleKind::Strings, 60, true, true);
    LOAD(inst_str_66, SampleKind::Strings, 66, true, true);
    LOAD(inst_str_72, SampleKind::Strings, 72, true, true);

    LOAD(inst_org_36, SampleKind::Organ, 36, true, true);
    LOAD(inst_org_48, SampleKind::Organ, 48, true, true);
    LOAD(inst_org_60, SampleKind::Organ, 60, true, true);
    LOAD(inst_org_64, SampleKind::Organ, 64, true, true);
    LOAD(inst_org_72, SampleKind::Organ, 72, true, true);

    LOAD(inst_plk_36, SampleKind::Pluck, 36, true, false);
    LOAD(inst_plk_48, SampleKind::Pluck, 48, true, false);
    LOAD(inst_plk_55, SampleKind::Pluck, 55, true, false);
    LOAD(inst_plk_60, SampleKind::Pluck, 60, true, false);
    LOAD(inst_plk_67, SampleKind::Pluck, 67, true, false);
#undef LOAD
}
