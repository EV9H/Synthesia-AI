#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <string>
#include <vector>

enum class SampleKind
{
    Off = 0,
    Piano,
    Ep,
    Strings,
    Organ,
    Pluck,
    Bass,
    Brass,
    Woodwind,
    Steel
};

class InstrumentLibrary
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> data;
        double sourceRate{44100.0};
        int rootNote{60};
        int velLayer{2};   // 0 soft, 1 mid, 2 hard
        int rr{0};
        bool isRelease{false};
        bool loopable{false};
        int loopStart{0};
        int loopEnd{0};
    };

    static InstrumentLibrary& get();
    void ensureLoaded();

    const Sample* find(SampleKind kind, int midiNote, float velocity) const;
    const Sample* findRelease(SampleKind kind, int midiNote, float velocity) const;

private:
    void load();
    void loadOne(const char* key, SampleKind kind, int rootNote, int velLayer, int rr,
                 bool isRelease, bool loopable, const char* data, int size);

    static constexpr int kKindCount = 10;
    bool loaded{false};
    mutable unsigned roundRobin{0};
    std::map<std::string, Sample> table;
    std::vector<const Sample*> byKind[kKindCount];
};
