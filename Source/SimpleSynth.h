#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Generators/MidiGenerator.h"
#include "InstrumentLibrary.h"
#include "SynthPatch.h"
#include <vector>

class PartSound : public juce::SynthesiserSound
{
public:
    PartSound(Part p, SynthPatch& patchToFollow)
        : part(p), patch(patchToFollow) {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int channel) override { return channel == partChannel(part); }

    const Part part;
    SynthPatch& patch;
};

class PartVoice : public juce::SynthesiserVoice
{
public:
    PartVoice();

    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    void nextStereo(float& left, float& right);
    double oscSample(const OscSettings& osc, double* phases, double hz, double dtBase, double pw) const;
    void readInstSample(double hz, float& left, float& right);
    void updateBend(int wheelValue);
    static double polyBlep(double t, double dt);
    static double wrap(double phase);
    static double waveAt(OscWave wave, double phase, double dt, double pw, double fmRatio, double fmIndex);

    SynthPatch* patch{nullptr};
    juce::ADSR ampEnvelope;
    juce::ADSR filtEnvelope;
    juce::dsp::StateVariableTPTFilter<float> filter;
    double preparedSampleRate{0.0};

    double targetHz{440.0};
    double currentHz{440.0};
    double bendRatio{1.0};
    double noteHz{440.0};
    int midiNote{60};
    float velocity{0.8f};
    double noteTime{0.0};
    double lfoPhase{0.0};
    double wowPhase{0.0};
    juce::uint32 noiseState{1};

    static constexpr int kMaxUnison = 7;
    double osc1Phase[kMaxUnison]{};
    double osc2Phase[kMaxUnison]{};
    double subPhase{0.0};
    double fmPhase{0.0};

    const InstrumentLibrary::Sample* instSample{nullptr};
    const InstrumentLibrary::Sample* instRelease{nullptr};
    double instPos{0.0};
    double relPos{0.0};
    bool playingRelease{false};

    static void lerpSample(const InstrumentLibrary::Sample* sample, double pos,
                           double transpose, float& left, float& right);
};

class DrumSound : public juce::SynthesiserSound
{
public:
    explicit DrumSound(Genre& kitToFollow) : kit(kitToFollow) {}

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int channel) override { return channel == 10; }

    Genre& kit;
};

class DrumVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote(float, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>&, int startSample, int numSamples) override;

private:
    float nextSample();
    float nextSynthSample();
    float noise();
    void updateSampleIncrement();

    enum class Piece { Kick, Snare, Clap, ClosedHat, OpenHat, Tom, Crash, Ride, Perc };

    Piece piece{Piece::Kick};
    Genre kit{Genre::Pop};
    double noteTime{0.0};
    double bodyPhase{0.0};
    double clickPhase{0.0};
    double bendRatio{1.0};
    double kickStartHz{150.0};
    double kickEndHz{50.0};
    double kickPitchRate{20.0};
    double kickAmpRate{8.0};
    double kickClick{0.35};
    double kickDrive{1.6};
    double toneHz{180.0};
    double toneMix{0.45};
    double noiseAmp{0.5};
    double noiseDecay{25.0};
    double bodyDecay{12.0};
    double hpState{0.0};
    double lpState{0.0};
    double bpState{0.0};
    double kitLpState{0.0};
    double kitLpCoeff{1.0};
    double env{1.0};
    double decayMul{0.999};
    float level{0.0f};
    float panL{0.707f};
    float panR{0.707f};
    float hatAirLp{0.0f};
    bool brightPiece{false};
    juce::uint32 noiseState{1};
    bool honourNoteOff{false};

    const juce::AudioBuffer<float>* sampleData{nullptr};
    double sampleRateOfFile{44100.0};
    double samplePos{0.0};
    double sampleIncrement{1.0};
    double tomRatio{1.0};
    bool usingSample{false};
};

class SimpleSynth
{
public:
    static constexpr double releaseSeconds = 2.4;

    SimpleSynth();
    ~SimpleSynth() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void setKit(Genre genre);
    void setTempo(double bpm);
    Genre getKit() const { return currentKit; }

    SynthPatch getPatch(Part part) const;
    void setPatch(Part part, const SynthPatch& patch);
    MixSettings getMix() const { return mix; }
    void setMix(const MixSettings& settings);
    void setSidechainAmount(float amount);
    void loadGenreDefaults(Genre genre);
    void loadNeutralPreview();
    int getSoundVersion() const { return soundVersion; }

    void writeSoundXml(juce::XmlElement& xml) const;
    void readSoundXml(const juce::XmlElement& xml);

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        const juce::MidiBuffer& midiMessages,
                        int startSample,
                        int numSamples);

private:
    void setupVoices();
    void prepareFx(double sampleRate, int samplesPerBlock);
    void applyPartInserts(int partIndex, juce::AudioBuffer<float>& buffer, int start, int num);
    void applyBusMix(int start, int num, const juce::MidiBuffer& drumMidi);
    void applyMaster(juce::AudioBuffer<float>& buffer, int start, int num);
    void ensureAuxSize(int start, int num);

    juce::Synthesiser pitched[4];
    juce::Synthesiser drums;
    SynthPatch patches[4];
    MixSettings mix;
    Genre currentKit{Genre::Pop};
    double currentSampleRate{44100.0};
    double tempoBpm{120.0};
    int currentBlockSize{512};
    int soundVersion{0};

    juce::AudioBuffer<float> partBuffer;
    juce::AudioBuffer<float> musicBus;
    juce::AudioBuffer<float> bassBus;
    juce::AudioBuffer<float> drumBus;
    juce::AudioBuffer<float> delaySend;
    juce::AudioBuffer<float> reverbSend;
    juce::AudioBuffer<float> dryBackup;

    juce::dsp::Chorus<float> chorus[4];
    juce::dsp::Reverb reverb;
    std::vector<float> delayBufL, delayBufR;
    int delayWrite{0};
    float partHpL[4]{}, partHpR[4]{};
    float partMudLpL[4]{}, partMudBpL[4]{}, partMudLpR[4]{}, partMudBpR[4]{};
    float partAirLpL[4]{}, partAirLpR[4]{};
    float bassMonoLpL{0.0f}, bassMonoLpR{0.0f};
    float musicHaas[48]{};
    int haasWrite{0};
    float hpStateL{0.0f}, hpStateR{0.0f};
    float shelfStateL{0.0f}, shelfStateR{0.0f};
    float kickLp{0.0f};
    float kickPulse{0.0f};
    float scEnv{0.0f};
    float duckSmooth{0.0f};
    float drumEnv{0.0f};
    float bassEnv{0.0f};
    float musicEnv{0.0f};
    float masterEnv{0.0f};
    float limitEnv{0.0f};
    juce::uint32 vinylState{1234567};
    double vinylPhase{0.0};
};
