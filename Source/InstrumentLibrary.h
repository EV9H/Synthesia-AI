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
    Pluck
};

class InstrumentLibrary
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> data;
        double sourceRate{44100.0};
        int rootNote{60};
        bool hard{true};
        bool loopable{false};
        int loopStart{0};
        int loopEnd{0};
    };

    static InstrumentLibrary& get();
    void ensureLoaded();

    const Sample* find(SampleKind kind, int midiNote, float velocity) const;

private:
    void load();
    void loadOne(const char* key, SampleKind kind, int rootNote, bool hard, bool loopable,
                 const char* data, int size);

    bool loaded{false};
    std::map<std::string, Sample> table;
    std::vector<const Sample*> byKind[6];
};
