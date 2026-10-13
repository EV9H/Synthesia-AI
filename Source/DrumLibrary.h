#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Generators/MidiGenerator.h"
#include <map>
#include <string>

enum class DrumPiece { Kick, Snare, Clap, ClosedHat, OpenHat, Tom, Crash, Ride, Perc };

// Embedded one-shots, keyed by genre kit. Missing slots fall back to the synth voice.
class DrumLibrary
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> data;
        double sourceRate{44100.0};
    };

    static DrumLibrary& get();
    void ensureLoaded();

    const Sample* find(Genre genre, DrumPiece piece, float velocity) const;
    float gainFor(Genre genre, DrumPiece piece) const;
    double kitLowpassCoeff(Genre genre) const;

    static DrumPiece pieceFromNote(int midiNote);
    static bool isClosedHat(int midiNote);
    static bool isOpenHat(int midiNote);
    static int tomRootNote() { return 45; }

private:
    void load();
    void loadOne(const char* key, const char* data, int size);
    const Sample* lookup(const char* prefix, const char* piece, bool preferHard) const;

    bool loaded{false};
    std::map<std::string, Sample> table;
};
