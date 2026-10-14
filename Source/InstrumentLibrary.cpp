#include "InstrumentLibrary.h"
#include "DrumBinaryData.h"
#include <cmath>
#include <memory>
#include <vector>

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

    const int wantVel = velocity < 0.40f ? 0 : (velocity < 0.70f ? 1 : 2);
    const Sample* best = nullptr;
    int bestScore = 100000;
    for (const Sample* s : list)
    {
        if (s->isRelease)
            continue;
        const int score = std::abs(s->rootNote - midiNote) * 4 + std::abs(s->velLayer - wantVel) * 3;
        if (score < bestScore)
        {
            bestScore = score;
            best = s;
        }
    }
    if (best == nullptr)
        return nullptr;

    std::vector<const Sample*> variants;
    for (const Sample* s : list)
    {
        if (s->isRelease)
            continue;
        if (s->rootNote == best->rootNote && s->velLayer == best->velLayer)
            variants.push_back(s);
    }
    if (variants.empty())
        return best;
    return variants[(roundRobin++) % variants.size()];
}

const InstrumentLibrary::Sample* InstrumentLibrary::findRelease(SampleKind kind, int midiNote, float velocity) const
{
    if (kind == SampleKind::Off)
        return nullptr;

    const auto& list = byKind[static_cast<int>(kind)];
    const int wantVel = velocity < 0.55f ? 0 : 2;
    const Sample* best = nullptr;
    int bestScore = 100000;
    for (const Sample* s : list)
    {
        if (!s->isRelease)
            continue;
        const int score = std::abs(s->rootNote - midiNote) * 4 + std::abs(s->velLayer - wantVel);
        if (score < bestScore)
        {
            bestScore = score;
            best = s;
        }
    }
    return best;
}

void InstrumentLibrary::loadOne(const char* key, SampleKind kind, int rootNote, int velLayer, int rr,
                                bool isRelease, bool loopable, const char* data, int size)
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
    sample.velLayer = velLayer;
    sample.rr = rr;
    sample.isRelease = isRelease;
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

#define LOAD(name, kind, note, vel, rr, rel, loop) \
    loadOne(#name, kind, note, vel, rr, rel, loop, DrumBinaryData::name##_wav, DrumBinaryData::name##_wavSize)

    LOAD(inst_piano_36_soft, SampleKind::Piano, 36, 0, 0, false, false);
    LOAD(inst_piano_36_mid,  SampleKind::Piano, 36, 1, 0, false, false);
    LOAD(inst_piano_36_hard, SampleKind::Piano, 36, 2, 0, false, false);
    LOAD(inst_piano_42_soft, SampleKind::Piano, 42, 0, 0, false, false);
    LOAD(inst_piano_42_mid,  SampleKind::Piano, 42, 1, 0, false, false);
    LOAD(inst_piano_42_hard, SampleKind::Piano, 42, 2, 0, false, false);
    LOAD(inst_piano_48_soft, SampleKind::Piano, 48, 0, 0, false, false);
    LOAD(inst_piano_48_mid,  SampleKind::Piano, 48, 1, 0, false, false);
    LOAD(inst_piano_48_hard, SampleKind::Piano, 48, 2, 0, false, false);
    LOAD(inst_piano_54_soft, SampleKind::Piano, 54, 0, 0, false, false);
    LOAD(inst_piano_54_mid,  SampleKind::Piano, 54, 1, 0, false, false);
    LOAD(inst_piano_54_hard, SampleKind::Piano, 54, 2, 0, false, false);
    LOAD(inst_piano_60_soft, SampleKind::Piano, 60, 0, 0, false, false);
    LOAD(inst_piano_60_mid,  SampleKind::Piano, 60, 1, 0, false, false);
    LOAD(inst_piano_60_hard, SampleKind::Piano, 60, 2, 0, false, false);
    LOAD(inst_piano_66_soft, SampleKind::Piano, 66, 0, 0, false, false);
    LOAD(inst_piano_66_mid,  SampleKind::Piano, 66, 1, 0, false, false);
    LOAD(inst_piano_66_hard, SampleKind::Piano, 66, 2, 0, false, false);
    LOAD(inst_piano_72_soft, SampleKind::Piano, 72, 0, 0, false, false);
    LOAD(inst_piano_72_mid,  SampleKind::Piano, 72, 1, 0, false, false);
    LOAD(inst_piano_72_hard, SampleKind::Piano, 72, 2, 0, false, false);
    LOAD(inst_piano_78_soft, SampleKind::Piano, 78, 0, 0, false, false);
    LOAD(inst_piano_78_mid,  SampleKind::Piano, 78, 1, 0, false, false);
    LOAD(inst_piano_78_hard, SampleKind::Piano, 78, 2, 0, false, false);
    LOAD(inst_piano_84_soft, SampleKind::Piano, 84, 0, 0, false, false);
    LOAD(inst_piano_84_mid,  SampleKind::Piano, 84, 1, 0, false, false);
    LOAD(inst_piano_84_hard, SampleKind::Piano, 84, 2, 0, false, false);
    LOAD(inst_piano_36_rel, SampleKind::Piano, 36, 2, 0, true, false);
    LOAD(inst_piano_48_rel, SampleKind::Piano, 48, 2, 0, true, false);
    LOAD(inst_piano_60_rel, SampleKind::Piano, 60, 2, 0, true, false);
    LOAD(inst_piano_72_rel, SampleKind::Piano, 72, 2, 0, true, false);
    LOAD(inst_piano_rel_soft, SampleKind::Piano, 60, 0, 0, true, false);
    LOAD(inst_piano_rel_hard, SampleKind::Piano, 60, 2, 0, true, false);

    LOAD(inst_ep_33_soft, SampleKind::Ep, 33, 0, 0, false, false);
    LOAD(inst_ep_33_hard, SampleKind::Ep, 33, 2, 0, false, false);
    LOAD(inst_ep_40_soft, SampleKind::Ep, 40, 0, 0, false, false);
    LOAD(inst_ep_40_hard, SampleKind::Ep, 40, 2, 0, false, false);
    LOAD(inst_ep_45_soft, SampleKind::Ep, 45, 0, 0, false, false);
    LOAD(inst_ep_45_hard, SampleKind::Ep, 45, 2, 0, false, false);
    LOAD(inst_ep_51_soft, SampleKind::Ep, 51, 0, 0, false, false);
    LOAD(inst_ep_51_hard, SampleKind::Ep, 51, 2, 0, false, false);
    LOAD(inst_ep_56_soft, SampleKind::Ep, 56, 0, 0, false, false);
    LOAD(inst_ep_56_hard, SampleKind::Ep, 56, 2, 0, false, false);
    LOAD(inst_ep_61_soft, SampleKind::Ep, 61, 0, 0, false, false);
    LOAD(inst_ep_61_hard, SampleKind::Ep, 61, 2, 0, false, false);
    LOAD(inst_ep_67_soft, SampleKind::Ep, 67, 0, 0, false, false);
    LOAD(inst_ep_67_hard, SampleKind::Ep, 67, 2, 0, false, false);
    LOAD(inst_ep_73_soft, SampleKind::Ep, 73, 0, 0, false, false);
    LOAD(inst_ep_73_hard, SampleKind::Ep, 73, 2, 0, false, false);
    LOAD(inst_ep_79_soft, SampleKind::Ep, 79, 0, 0, false, false);
    LOAD(inst_ep_79_hard, SampleKind::Ep, 79, 2, 0, false, false);

    LOAD(inst_str_24_soft, SampleKind::Strings, 24, 0, 0, false, true);
    LOAD(inst_str_24_hard, SampleKind::Strings, 24, 2, 0, false, true);
    LOAD(inst_str_43_soft, SampleKind::Strings, 43, 0, 0, false, true);
    LOAD(inst_str_43_hard, SampleKind::Strings, 43, 2, 0, false, true);
    LOAD(inst_str_48_soft, SampleKind::Strings, 48, 0, 0, false, true);
    LOAD(inst_str_48_hard, SampleKind::Strings, 48, 2, 0, false, true);
    LOAD(inst_str_60_soft, SampleKind::Strings, 60, 0, 0, false, true);
    LOAD(inst_str_60_hard, SampleKind::Strings, 60, 2, 0, false, true);
    LOAD(inst_str_64_soft, SampleKind::Strings, 64, 0, 0, false, true);
    LOAD(inst_str_64_hard, SampleKind::Strings, 64, 2, 0, false, true);
    LOAD(inst_str_67_soft, SampleKind::Strings, 67, 0, 0, false, true);
    LOAD(inst_str_67_hard, SampleKind::Strings, 67, 2, 0, false, true);
    LOAD(inst_str_74_soft, SampleKind::Strings, 74, 0, 0, false, true);
    LOAD(inst_str_74_hard, SampleKind::Strings, 74, 2, 0, false, true);

    LOAD(inst_org_36_soft, SampleKind::Organ, 36, 0, 0, false, true);
    LOAD(inst_org_36_hard, SampleKind::Organ, 36, 2, 0, false, true);
    LOAD(inst_org_48_soft, SampleKind::Organ, 48, 0, 0, false, true);
    LOAD(inst_org_48_hard, SampleKind::Organ, 48, 2, 0, false, true);
    LOAD(inst_org_60_soft, SampleKind::Organ, 60, 0, 0, false, true);
    LOAD(inst_org_60_hard, SampleKind::Organ, 60, 2, 0, false, true);
    LOAD(inst_org_72_soft, SampleKind::Organ, 72, 0, 0, false, true);
    LOAD(inst_org_72_hard, SampleKind::Organ, 72, 2, 0, false, true);
    LOAD(inst_org_84_soft, SampleKind::Organ, 84, 0, 0, false, true);
    LOAD(inst_org_84_hard, SampleKind::Organ, 84, 2, 0, false, true);

    LOAD(inst_plk_40_hard, SampleKind::Pluck, 40, 2, 0, false, false);
    LOAD(inst_plk_43_hard, SampleKind::Pluck, 43, 2, 0, false, false);
    LOAD(inst_plk_48_hard, SampleKind::Pluck, 48, 2, 0, false, false);
    LOAD(inst_plk_52_hard, SampleKind::Pluck, 52, 2, 0, false, false);
    LOAD(inst_plk_55_hard, SampleKind::Pluck, 55, 2, 0, false, false);
    LOAD(inst_plk_60_hard, SampleKind::Pluck, 60, 2, 0, false, false);
    LOAD(inst_plk_64_hard, SampleKind::Pluck, 64, 2, 0, false, false);
    LOAD(inst_plk_67_hard, SampleKind::Pluck, 67, 2, 0, false, false);
    LOAD(inst_plk_72_hard, SampleKind::Pluck, 72, 2, 0, false, false);
    LOAD(inst_plk_76_hard, SampleKind::Pluck, 76, 2, 0, false, false);

    LOAD(inst_bass_28_soft, SampleKind::Bass, 28, 0, 0, false, false);
    LOAD(inst_bass_28_hard, SampleKind::Bass, 28, 2, 0, false, false);
    LOAD(inst_bass_29_soft, SampleKind::Bass, 29, 0, 0, false, false);
    LOAD(inst_bass_29_hard, SampleKind::Bass, 29, 2, 0, false, false);
    LOAD(inst_bass_30_soft, SampleKind::Bass, 30, 0, 0, false, false);
    LOAD(inst_bass_30_hard, SampleKind::Bass, 30, 2, 0, false, false);
    LOAD(inst_bass_31_soft, SampleKind::Bass, 31, 0, 0, false, false);
    LOAD(inst_bass_31_hard, SampleKind::Bass, 31, 2, 0, false, false);
    LOAD(inst_bass_32_soft, SampleKind::Bass, 32, 0, 0, false, false);
    LOAD(inst_bass_32_hard, SampleKind::Bass, 32, 2, 0, false, false);
    LOAD(inst_bass_33_soft, SampleKind::Bass, 33, 0, 0, false, false);
    LOAD(inst_bass_33_hard, SampleKind::Bass, 33, 2, 0, false, false);
    LOAD(inst_bass_34_soft, SampleKind::Bass, 34, 0, 0, false, false);
    LOAD(inst_bass_34_hard, SampleKind::Bass, 34, 2, 0, false, false);
    LOAD(inst_bass_35_soft, SampleKind::Bass, 35, 0, 0, false, false);
    LOAD(inst_bass_35_hard, SampleKind::Bass, 35, 2, 0, false, false);
    LOAD(inst_bass_36_soft, SampleKind::Bass, 36, 0, 0, false, false);
    LOAD(inst_bass_36_hard, SampleKind::Bass, 36, 2, 0, false, false);
    LOAD(inst_bass_37_soft, SampleKind::Bass, 37, 0, 0, false, false);
    LOAD(inst_bass_37_hard, SampleKind::Bass, 37, 2, 0, false, false);
    LOAD(inst_bass_38_soft, SampleKind::Bass, 38, 0, 0, false, false);
    LOAD(inst_bass_38_hard, SampleKind::Bass, 38, 2, 0, false, false);
    LOAD(inst_bass_39_soft, SampleKind::Bass, 39, 0, 0, false, false);
    LOAD(inst_bass_39_hard, SampleKind::Bass, 39, 2, 0, false, false);
    LOAD(inst_bass_40_hard, SampleKind::Bass, 40, 2, 0, false, false);

    LOAD(inst_brs_29_soft, SampleKind::Brass, 29, 0, 0, false, true);
    LOAD(inst_brs_29_hard, SampleKind::Brass, 29, 2, 0, false, true);
    LOAD(inst_brs_34_soft, SampleKind::Brass, 34, 0, 0, false, true);
    LOAD(inst_brs_34_hard, SampleKind::Brass, 34, 2, 0, false, true);
    LOAD(inst_brs_41_soft, SampleKind::Brass, 41, 0, 0, false, true);
    LOAD(inst_brs_41_hard, SampleKind::Brass, 41, 2, 0, false, true);
    LOAD(inst_brs_48_soft, SampleKind::Brass, 48, 0, 0, false, true);
    LOAD(inst_brs_48_hard, SampleKind::Brass, 48, 2, 0, false, true);
    LOAD(inst_brs_55_soft, SampleKind::Brass, 55, 0, 0, false, true);
    LOAD(inst_brs_55_hard, SampleKind::Brass, 55, 2, 0, false, true);
    LOAD(inst_brs_62_soft, SampleKind::Brass, 62, 0, 0, false, true);
    LOAD(inst_brs_62_hard, SampleKind::Brass, 62, 2, 0, false, true);

    LOAD(inst_ww_48_soft, SampleKind::Woodwind, 48, 0, 0, false, true);
    LOAD(inst_ww_52_soft, SampleKind::Woodwind, 52, 0, 0, false, true);
    LOAD(inst_ww_64_soft, SampleKind::Woodwind, 64, 0, 0, false, true);
    LOAD(inst_ww_64_soft_rr1, SampleKind::Woodwind, 64, 0, 1, false, true);
    LOAD(inst_ww_69_soft, SampleKind::Woodwind, 69, 0, 0, false, true);
    LOAD(inst_ww_72_soft, SampleKind::Woodwind, 72, 0, 0, false, true);
    LOAD(inst_ww_84_soft, SampleKind::Woodwind, 84, 0, 0, false, true);
    LOAD(inst_ww_96_soft, SampleKind::Woodwind, 96, 0, 0, false, true);

    LOAD(inst_stl_52_soft, SampleKind::Steel, 52, 0, 0, false, false);
    LOAD(inst_stl_52_soft_rr2, SampleKind::Steel, 52, 0, 2, false, false);
    LOAD(inst_stl_52_hard, SampleKind::Steel, 52, 2, 0, false, false);
    LOAD(inst_stl_52_hard_rr2, SampleKind::Steel, 52, 2, 2, false, false);
    LOAD(inst_stl_52_rel, SampleKind::Steel, 52, 2, 0, true, false);
    LOAD(inst_stl_57_soft, SampleKind::Steel, 57, 0, 0, false, false);
    LOAD(inst_stl_57_soft_rr2, SampleKind::Steel, 57, 0, 2, false, false);
    LOAD(inst_stl_57_hard, SampleKind::Steel, 57, 2, 0, false, false);
    LOAD(inst_stl_57_hard_rr2, SampleKind::Steel, 57, 2, 2, false, false);
    LOAD(inst_stl_57_rel, SampleKind::Steel, 57, 2, 0, true, false);
    LOAD(inst_stl_60_soft, SampleKind::Steel, 60, 0, 0, false, false);
    LOAD(inst_stl_60_soft_rr2, SampleKind::Steel, 60, 0, 2, false, false);
    LOAD(inst_stl_60_hard, SampleKind::Steel, 60, 2, 0, false, false);
    LOAD(inst_stl_60_hard_rr2, SampleKind::Steel, 60, 2, 2, false, false);
    LOAD(inst_stl_60_rel, SampleKind::Steel, 60, 2, 0, true, false);
    LOAD(inst_stl_63_soft, SampleKind::Steel, 63, 0, 0, false, false);
    LOAD(inst_stl_63_soft_rr2, SampleKind::Steel, 63, 0, 2, false, false);
    LOAD(inst_stl_63_hard, SampleKind::Steel, 63, 2, 0, false, false);
    LOAD(inst_stl_63_hard_rr2, SampleKind::Steel, 63, 2, 2, false, false);
    LOAD(inst_stl_63_rel, SampleKind::Steel, 63, 2, 0, true, false);
    LOAD(inst_stl_69_soft, SampleKind::Steel, 69, 0, 0, false, false);
    LOAD(inst_stl_69_soft_rr2, SampleKind::Steel, 69, 0, 2, false, false);
    LOAD(inst_stl_69_hard, SampleKind::Steel, 69, 2, 0, false, false);
    LOAD(inst_stl_69_hard_rr2, SampleKind::Steel, 69, 2, 2, false, false);
    LOAD(inst_stl_69_rel, SampleKind::Steel, 69, 2, 0, true, false);
    LOAD(inst_stl_72_soft, SampleKind::Steel, 72, 0, 0, false, false);
    LOAD(inst_stl_72_soft_rr2, SampleKind::Steel, 72, 0, 2, false, false);
    LOAD(inst_stl_72_hard, SampleKind::Steel, 72, 2, 0, false, false);
    LOAD(inst_stl_72_hard_rr2, SampleKind::Steel, 72, 2, 2, false, false);
    LOAD(inst_stl_72_rel, SampleKind::Steel, 72, 2, 0, true, false);
    LOAD(inst_stl_78_soft, SampleKind::Steel, 78, 0, 0, false, false);
    LOAD(inst_stl_78_soft_rr2, SampleKind::Steel, 78, 0, 2, false, false);
    LOAD(inst_stl_78_hard, SampleKind::Steel, 78, 2, 0, false, false);
    LOAD(inst_stl_78_hard_rr2, SampleKind::Steel, 78, 2, 2, false, false);
    LOAD(inst_stl_78_rel, SampleKind::Steel, 78, 2, 0, true, false);
    LOAD(inst_stl_84_soft, SampleKind::Steel, 84, 0, 0, false, false);
    LOAD(inst_stl_84_soft_rr2, SampleKind::Steel, 84, 0, 2, false, false);
    LOAD(inst_stl_84_hard, SampleKind::Steel, 84, 2, 0, false, false);
    LOAD(inst_stl_84_hard_rr2, SampleKind::Steel, 84, 2, 2, false, false);
    LOAD(inst_stl_84_rel, SampleKind::Steel, 84, 2, 0, true, false);
    LOAD(inst_stl_90_soft, SampleKind::Steel, 90, 0, 0, false, false);
    LOAD(inst_stl_90_soft_rr2, SampleKind::Steel, 90, 0, 2, false, false);
    LOAD(inst_stl_90_hard, SampleKind::Steel, 90, 2, 0, false, false);
    LOAD(inst_stl_90_hard_rr2, SampleKind::Steel, 90, 2, 2, false, false);
    LOAD(inst_stl_90_rel, SampleKind::Steel, 90, 2, 0, true, false);
#undef LOAD
}
