#pragma once

#include "Generators/GenreProfiles.h"
#include <juce_core/juce_core.h>

enum class InspireMood
{
    Chill = 0,
    Dark,
    Happy,
    Epic,
    Dreamy
};

constexpr int numMoods = 5;

inline const char* moodName(InspireMood mood)
{
    switch (mood)
    {
        case InspireMood::Chill:  return "Chill";
        case InspireMood::Dark:   return "Dark";
        case InspireMood::Happy:  return "Happy";
        case InspireMood::Epic:   return "Epic";
        case InspireMood::Dreamy: return "Dreamy";
    }
    return "Chill";
}

inline const char* moodCaption(InspireMood mood)
{
    switch (mood)
    {
        case InspireMood::Chill:  return "chill";
        case InspireMood::Dark:   return "melancholic";
        case InspireMood::Happy:  return "sunlit";
        case InspireMood::Epic:   return "epic";
        case InspireMood::Dreamy: return "dreamy";
    }
    return "chill";
}

struct InspireLocks
{
    bool genre{false};
    bool key{false};
    bool tempo{false};
    bool mood{false};
};

struct InspireIdea
{
    MidiGenerationParams params;
    InspireMood mood{InspireMood::Chill};
    juce::String adjective;
    juce::String genreLabel;
    juce::String keyLabel;
    juce::String tempoLabel;
    juce::String moodLabel;
    juce::String caption;
};

inline juce::String inspireShortGenre(Genre genre)
{
    switch (genre)
    {
        case Genre::Pop:       return "pop";
        case Genre::LoFiJazz:  return "lo-fi";
        case Genre::House:     return "house";
        case Genre::Trap:      return "trap";
        case Genre::RnB:       return "R&B";
        case Genre::Cinematic: return "film";
    }
    return "pop";
}

inline juce::String inspireScaleName(ScaleType scale)
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

inline juce::Range<int> inspireTempoRange(Genre genre)
{
    switch (genre)
    {
        case Genre::Pop:       return { 100, 118 };
        case Genre::LoFiJazz:  return { 74, 90 };
        case Genre::House:     return { 120, 128 };
        case Genre::Trap:      return { 132, 148 };
        case Genre::RnB:       return { 80, 96 };
        case Genre::Cinematic: return { 68, 88 };
    }
    return { 90, 120 };
}

inline const char* inspireAdjective(Genre genre, InspireMood mood, juce::Random& rng)
{
    const char* pop[] = { "Bright", "Radio", "Glossy", "Catchy" };
    const char* lofi[] = { "Dusty", "Hazy", "Tape", "Warm" };
    const char* house[] = { "Peak", "Night", "Rolling", "Warehouse" };
    const char* trap[] = { "Chrome", "Night", "Heavy", "Icy" };
    const char* rnb[] = { "Velvet", "Late", "Soft", "Honey" };
    const char* cine[] = { "Wide", "Fog", "Grand", "Quiet" };
    const char** list = pop;
    int n = 4;
    switch (genre)
    {
        case Genre::Pop:       list = pop; break;
        case Genre::LoFiJazz:  list = lofi; break;
        case Genre::House:     list = house; break;
        case Genre::Trap:      list = trap; break;
        case Genre::RnB:       list = rnb; break;
        case Genre::Cinematic: list = cine; break;
    }
    juce::ignoreUnused(mood);
    return list[rng.nextInt(n)];
}

inline ScaleType inspireScaleForMood(InspireMood mood, juce::Random& rng)
{
    switch (mood)
    {
        case InspireMood::Happy:
        {
            const ScaleType opts[] = { ScaleType::Major, ScaleType::Mixolydian, ScaleType::Pentatonic };
            return opts[rng.nextInt(3)];
        }
        case InspireMood::Dark:
            return ScaleType::Minor;
        case InspireMood::Chill:
        {
            const ScaleType opts[] = { ScaleType::Dorian, ScaleType::Minor, ScaleType::Mixolydian };
            return opts[rng.nextInt(3)];
        }
        case InspireMood::Epic:
            return rng.nextBool() ? ScaleType::Minor : ScaleType::Major;
        case InspireMood::Dreamy:
        {
            const ScaleType opts[] = { ScaleType::Dorian, ScaleType::Pentatonic, ScaleType::Major };
            return opts[rng.nextInt(3)];
        }
    }
    return ScaleType::Minor;
}

inline bool inspireUsesMinorPool(ScaleType scale)
{
    return scale == ScaleType::Minor || scale == ScaleType::Dorian;
}

inline InspireIdea rollInspire(const InspireIdea* previous, const InspireLocks& locks, juce::Random& rng)
{
    InspireIdea idea;
    if (previous != nullptr)
        idea = *previous;

    idea.mood = (locks.mood && previous != nullptr)
        ? previous->mood
        : static_cast<InspireMood>(rng.nextInt(numMoods));

    const Genre genre = (locks.genre && previous != nullptr)
        ? previous->params.genre
        : static_cast<Genre>(rng.nextInt(numGenres));
    const auto& profile = getGenreProfile(genre);

    const int root = (locks.key && previous != nullptr)
        ? previous->params.rootNote
        : rng.nextInt(12);
    const ScaleType scale = (locks.key && previous != nullptr)
        ? previous->params.scale
        : inspireScaleForMood(idea.mood, rng);

    auto tempoRange = inspireTempoRange(genre);
    int lo = tempoRange.getStart();
    int hi = tempoRange.getEnd();
    const int span = juce::jmax(1, hi - lo);
    if (idea.mood == InspireMood::Chill || idea.mood == InspireMood::Dreamy)
        hi = lo + juce::jmax(1, span / 2);
    else if (idea.mood == InspireMood::Happy || idea.mood == InspireMood::Epic)
        lo = lo + span / 3;
    else
    {
        lo = lo + span / 6;
        hi = hi - span / 8;
    }
    const double tempo = (locks.tempo && previous != nullptr)
        ? previous->params.tempo
        : (double) (lo + rng.nextInt(juce::jmax(1, hi - lo + 1)));

    const auto& pool = inspireUsesMinorPool(scale) ? profile.minorProgressions : profile.majorProgressions;
    const auto& fallback = profile.minorProgressions.empty() ? profile.majorProgressions : profile.minorProgressions;
    const auto& progressions = pool.empty() ? fallback : pool;
    const int progIndex = rng.nextInt(juce::jmax(1, (int) progressions.size()));
    const auto& progression = progressions[static_cast<size_t>(juce::jlimit(0, (int) progressions.size() - 1, progIndex))];

    const int nGrooves = juce::jmax(1, (int) profile.drums.grooves.size());
    const int grooveIndex = rng.nextInt(nGrooves);

    MidiGenerationParams params;
    if (previous != nullptr)
        params = previous->params;
    params.genre = genre;
    params.rootNote = root;
    params.scale = scale;
    params.tempo = tempo;
    params.quantization = profile.defaultGrid;
    params.part = Part::All;
    params.form = idea.mood == InspireMood::Epic ? ArrangementForm::Song : ArrangementForm::Loop;
    params.lengthInBars = idea.mood == InspireMood::Epic ? 16 : 8;
    params.chordText = progression.tokens;
    params.lockProgression = true;
    params.progression.clear();
    params.grooveIndex = grooveIndex;
    params.seed = 1u + (unsigned int) rng.nextInt(9998);
    params.importMode = ImportMode::None;
    params.stylePrompt.clear();
    params.planJson.clear();
    params.plannedSections.clear();

    switch (idea.mood)
    {
        case InspireMood::Chill:
            params.density = 0.34f;
            params.swing = juce::jmax(0.56, profile.swing);
            params.humanizeMs = juce::jmax(6.0, profile.drums.humanizeMs);
            params.motifContour = rng.nextBool() ? "static" : "falling";
            params.motifRhythm = "sparse";
            break;
        case InspireMood::Dark:
            params.density = 0.40f;
            params.swing = profile.swing;
            params.humanizeMs = profile.drums.humanizeMs;
            params.motifContour = "falling";
            params.motifRhythm = "sparse";
            break;
        case InspireMood::Happy:
            params.density = 0.78f;
            params.swing = 0.5;
            params.humanizeMs = 3.0;
            params.motifContour = "rising";
            params.motifRhythm = "straight";
            break;
        case InspireMood::Epic:
            params.density = 0.86f;
            params.swing = 0.5;
            params.humanizeMs = 3.0;
            params.motifContour = "arch";
            params.motifRhythm = "straight";
            break;
        case InspireMood::Dreamy:
            params.density = 0.30f;
            params.swing = juce::jmax(0.58, profile.swing);
            params.humanizeMs = juce::jmax(7.0, profile.drums.humanizeMs);
            params.motifContour = "arch";
            params.motifRhythm = rng.nextBool() ? "swung" : "sparse";
            break;
    }

    static const char* keyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    idea.params = params;
    idea.adjective = inspireAdjective(genre, idea.mood, rng);
    idea.genreLabel = idea.adjective + " " + inspireShortGenre(genre);
    idea.keyLabel = juce::String(keyNames[juce::jlimit(0, 11, root)]) + " " + inspireScaleName(scale);
    idea.tempoLabel = juce::String(juce::roundToInt(tempo)) + " bpm";
    idea.moodLabel = moodCaption(idea.mood);
    idea.caption = idea.genreLabel + "  ·  " + idea.keyLabel + "  ·  " + idea.tempoLabel
                   + "  ·  " + idea.moodLabel;
    return idea;
}
