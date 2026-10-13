#include "DrumLibrary.h"
#include "DrumBinaryData.h"

namespace
{
    const char* prefixFor(Genre genre)
    {
        switch (genre)
        {
            case Genre::House:     return "house";
            case Genre::Trap:      return "trap";
            case Genre::LoFiJazz:  return "lofi";
            case Genre::RnB:       return "rnb";
            case Genre::Cinematic: return "cine";
            case Genre::Pop:
            default:               return "pop";
        }
    }

    const char* pieceName(DrumPiece piece)
    {
        switch (piece)
        {
            case DrumPiece::Kick:      return "kick";
            case DrumPiece::Snare:     return "snare";
            case DrumPiece::Clap:      return "clap";
            case DrumPiece::ClosedHat: return "ch";
            case DrumPiece::OpenHat:   return "oh";
            case DrumPiece::Tom:       return "tom";
            case DrumPiece::Crash:     return "crash";
            case DrumPiece::Ride:      return "ride";
            case DrumPiece::Perc:      return "perc";
        }
        return "kick";
    }
}

DrumLibrary& DrumLibrary::get()
{
    static DrumLibrary library;
    return library;
}

void DrumLibrary::ensureLoaded()
{
    if (!loaded)
        load();
}

DrumPiece DrumLibrary::pieceFromNote(int midiNote)
{
    switch (midiNote)
    {
        case 35: case 36:                         return DrumPiece::Kick;
        case 38: case 40:                         return DrumPiece::Snare;
        case 39:                                  return DrumPiece::Clap;
        case 42: case 44:                         return DrumPiece::ClosedHat;
        case 46:                                  return DrumPiece::OpenHat;
        case 49: case 57:                         return DrumPiece::Crash;
        case 51: case 59:                         return DrumPiece::Ride;
        case 37: case 54: case 56: case 69: case 70: return DrumPiece::Perc;
        default:                                  return DrumPiece::Tom;
    }
}

bool DrumLibrary::isClosedHat(int midiNote)
{
    return midiNote == 42 || midiNote == 44;
}

bool DrumLibrary::isOpenHat(int midiNote)
{
    return midiNote == 46;
}

const DrumLibrary::Sample* DrumLibrary::lookup(const char* prefix, const char* piece, bool preferHard) const
{
    const std::string hard = std::string(prefix) + "_" + piece + "_hard";
    const std::string soft = std::string(prefix) + "_" + piece + "_soft";
    const std::string single = std::string(prefix) + "_" + piece;

    auto findKey = [this](const std::string& key) -> const Sample*
    {
        const auto it = table.find(key);
        return it != table.end() ? &it->second : nullptr;
    };

    if (preferHard)
    {
        if (auto* s = findKey(hard))   return s;
        if (auto* s = findKey(single)) return s;
        return findKey(soft);
    }

    if (auto* s = findKey(soft))   return s;
    if (auto* s = findKey(single)) return s;
    return findKey(hard);
}

const DrumLibrary::Sample* DrumLibrary::find(Genre genre, DrumPiece piece, float velocity) const
{
    const char* prefix = prefixFor(genre);
    const bool hard = velocity >= 0.55f;
    if (auto* sample = lookup(prefix, pieceName(piece), hard))
        return sample;

    if (piece == DrumPiece::Ride)
        return lookup(prefix, "crash", true);
    if (piece == DrumPiece::Clap)
        return lookup(prefix, "snare", hard);

    return nullptr;
}

float DrumLibrary::gainFor(Genre genre, DrumPiece piece) const
{
    float gain = 0.5f;
    switch (piece)
    {
        case DrumPiece::Kick:      gain = 0.52f; break;
        case DrumPiece::Snare:     gain = 0.48f; break;
        case DrumPiece::Clap:      gain = 0.42f; break;
        case DrumPiece::ClosedHat: gain = 0.32f; break;
        case DrumPiece::OpenHat:   gain = 0.34f; break;
        case DrumPiece::Tom:       gain = 0.40f; break;
        case DrumPiece::Crash:     gain = 0.26f; break;
        case DrumPiece::Ride:      gain = 0.28f; break;
        case DrumPiece::Perc:      gain = 0.30f; break;
    }

    switch (genre)
    {
        case Genre::House:
            if (piece == DrumPiece::Kick) gain = 0.48f;
            if (piece == DrumPiece::ClosedHat) gain = 0.36f;
            break;
        case Genre::Trap:
            if (piece == DrumPiece::Kick) gain = 0.40f;   // bass already carries the long 808
            if (piece == DrumPiece::ClosedHat) gain = 0.30f;
            break;
        case Genre::LoFiJazz:
            gain *= 0.92f;
            break;
        case Genre::Cinematic:
            if (piece == DrumPiece::Kick) gain = 0.50f;
            if (piece == DrumPiece::Crash) gain = 0.30f;
            break;
        case Genre::Pop:
        case Genre::RnB:
            break;
    }

    return gain;
}

double DrumLibrary::kitLowpassCoeff(Genre genre) const
{
    switch (genre)
    {
        case Genre::LoFiJazz:  return 0.42;
        case Genre::RnB:       return 0.55;
        case Genre::Cinematic: return 0.62;
        case Genre::Trap:      return 0.88;
        case Genre::House:     return 0.94;
        case Genre::Pop:
        default:               return 1.0;
    }
}

void DrumLibrary::loadOne(const char* key, const char* data, int size)
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
    const int length = static_cast<int>(reader->lengthInSamples);
    sample.data.setSize(1, juce::jmax(1, length));
    sample.data.clear();
    reader->read(&sample.data, 0, length, 0, true, true);
    table.emplace(key, std::move(sample));
}

void DrumLibrary::load()
{
    loaded = true;

#define LOAD(name) loadOne(#name, DrumBinaryData::name##_wav, DrumBinaryData::name##_wavSize)
    LOAD(house_kick); LOAD(house_snare); LOAD(house_clap); LOAD(house_ch); LOAD(house_oh);
    LOAD(house_tom); LOAD(house_crash); LOAD(house_ride); LOAD(house_perc);
    LOAD(trap_kick); LOAD(trap_snare); LOAD(trap_clap); LOAD(trap_ch); LOAD(trap_oh);
    LOAD(trap_tom); LOAD(trap_crash); LOAD(trap_perc);
    LOAD(lofi_kick_soft); LOAD(lofi_kick_hard); LOAD(lofi_snare_soft); LOAD(lofi_snare_hard);
    LOAD(lofi_clap); LOAD(lofi_ch); LOAD(lofi_oh); LOAD(lofi_tom); LOAD(lofi_crash);
    LOAD(lofi_ride); LOAD(lofi_perc);
    LOAD(pop_kick_soft); LOAD(pop_kick_hard); LOAD(pop_snare_soft); LOAD(pop_snare_hard);
    LOAD(pop_clap); LOAD(pop_ch); LOAD(pop_oh); LOAD(pop_tom); LOAD(pop_crash); LOAD(pop_perc);
    LOAD(rnb_kick_soft); LOAD(rnb_kick_hard); LOAD(rnb_snare_soft); LOAD(rnb_snare_hard);
    LOAD(rnb_clap); LOAD(rnb_ch); LOAD(rnb_oh); LOAD(rnb_tom); LOAD(rnb_crash); LOAD(rnb_perc);
    LOAD(cine_kick_soft); LOAD(cine_kick_hard); LOAD(cine_snare_soft); LOAD(cine_snare_hard);
    LOAD(cine_clap); LOAD(cine_ch); LOAD(cine_oh); LOAD(cine_tom); LOAD(cine_crash);
    LOAD(cine_ride); LOAD(cine_perc);
#undef LOAD
}
