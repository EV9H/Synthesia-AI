#pragma once

#include "Generators/MidiGenerator.h"
#include "InstrumentLibrary.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

enum class OscWave
{
    Sine = 0,
    Saw,
    Square,
    Triangle,
    Pulse,
    Noise,
    Tine   // FM electric-piano / bell
};

enum class FilterMode
{
    Off = 0,
    LowPass,
    HighPass,
    BandPass
};

struct OscSettings
{
    OscWave wave{OscWave::Saw};
    float mix{1.0f};
    float detuneCents{0.0f};
    int octave{0};
    int unison{1};
    float unisonDetune{8.0f};
    float pulseWidth{0.5f};
    float fmRatio{1.0f};
    float fmIndex{0.0f};
};

struct SynthPatch
{
    juce::String id;
    juce::String name;
    Part part{Part::Chords};

    OscSettings osc1, osc2;
    float subMix{0.0f};
    int subOctave{-1};
    float noiseMix{0.0f};
    float tineClick{0.0f};

    juce::ADSR::Parameters amp{0.01f, 0.2f, 0.7f, 0.25f};
    juce::ADSR::Parameters filterEnv{0.005f, 0.18f, 0.25f, 0.2f};

    FilterMode filterMode{FilterMode::LowPass};
    float cutoff{1800.0f};
    float resonance{0.35f};
    float filterEnvAmount{1.2f};  // octaves
    float keyTrack{0.35f};
    float velToCutoff{0.35f};

    float pitchEnvAmount{0.0f};   // semitones
    float pitchEnvDecay{0.08f};

    float lfoRate{5.0f};
    float lfoDelay{0.2f};
    float lfoToPitch{0.0f};       // cents
    float lfoToFilter{0.0f};      // octaves
    float lfoToPwm{0.0f};

    float glideSeconds{0.0f};
    float drive{0.15f};
    float chorusSend{0.0f};
    float delaySend{0.08f};
    float reverbSend{0.12f};
    float pan{0.0f};
    float gain{0.7f};

    float wowFlutter{0.0f};
    float tapeSat{0.0f};
    float vinylAmount{0.0f};

    SampleKind sampleKind{SampleKind::Off};
    float sampleMix{0.0f};   // 0 = synth only, 1 = sampled instrument

    void writeXml(juce::XmlElement& xml) const;
    static SynthPatch fromXml(const juce::XmlElement& xml);
};

class PatchLibrary
{
public:
    static juce::Array<SynthPatch> all();
    static juce::StringArray namesFor(Part part);
    static juce::StringArray idsFor(Part part);
    static SynthPatch find(const juce::String& id);
    static SynthPatch defaultFor(Genre genre, Part part);
    static int pitchedIndex(Part part);
};

struct BusComp
{
    float amount{0.4f};
    float attack{0.01f};    // seconds
    float release{0.12f};
    float ratio{2.4f};
    float thresh{0.28f};
};

struct MixSettings
{
    juce::String id;
    juce::String name;

    float sidechainAmount{0.35f};
    float sidechainAttack{0.012f};   // seconds
    float sidechainRelease{0.18f};
    float sidechainBass{0.7f};       // 0 = leave bass/808 unducked

    float compAmount{0.55f};         // UI macro 0-1
    float outputGain{1.15f};         // makeup into the true-peak limiter
    float musicWidth{1.35f};
    BusComp drum { 0.28f, 0.018f, 0.12f, 2.4f, 0.32f };
    BusComp bass { 0.26f, 0.022f, 0.16f, 2.2f, 0.34f };
    BusComp music { 0.22f, 0.028f, 0.18f, 2.0f, 0.36f };
    BusComp master { 0.22f, 0.040f, 0.22f, 1.6f, 0.42f };

    void writeXml(juce::XmlElement& xml) const;
    static MixSettings fromXml(const juce::XmlElement& xml);
};

class MixLibrary
{
public:
    static juce::Array<MixSettings> all();
    static juce::StringArray names();
    static juce::StringArray ids();
    static MixSettings find(const juce::String& id);
    static MixSettings defaultFor(Genre genre);
};
