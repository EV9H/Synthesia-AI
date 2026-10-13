#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Generators/MarkovGenerator.h"
#include "Generators/LLMGenerator.h"
#include "Generators/ArrangementGenerator.h"
#include "MidiExporter.h"
#include <cmath>
#include <thread>

namespace
{
    constexpr double ticksPerQuarterNote = 960.0;
}

SynthesiaAIProcessor::SynthesiaAIProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    generator = std::make_shared<ArrangementGenerator>();
}

SynthesiaAIProcessor::~SynthesiaAIProcessor()
{
}

const juce::String SynthesiaAIProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SynthesiaAIProcessor::acceptsMidi() const
{
    return true;
}

bool SynthesiaAIProcessor::producesMidi() const
{
    return true;
}

bool SynthesiaAIProcessor::isMidiEffect() const
{
    return false; // We're an instrument, not a pure MIDI effect
}

double SynthesiaAIProcessor::getTailLengthSeconds() const
{
    return SimpleSynth::releaseSeconds;
}

int SynthesiaAIProcessor::getNumPrograms()
{
    return 1;
}

int SynthesiaAIProcessor::getCurrentProgram()
{
    return 0;
}

void SynthesiaAIProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String SynthesiaAIProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void SynthesiaAIProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void SynthesiaAIProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    simpleSynth.prepare(sampleRate, samplesPerBlock);
    blockMidi.ensureSize(4096);
    previewBeat = 0.0;
    wasPlaying = false;
    hostWasPlaying = false;
}

void SynthesiaAIProcessor::releaseResources()
{
    simpleSynth.releaseResources();
}

bool SynthesiaAIProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SynthesiaAIProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    buffer.clear();
    const int numSamples = buffer.getNumSamples();

    double bpm = previewTempo.load();
    bool hostPlaying = false;
    double hostBeat = 0.0;

    if (auto* playHead = getPlayHead())
    {
        if (auto position = playHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (position->getIsPlaying())
            {
                if (auto ppq = position->getPpqPosition())
                {
                    hostPlaying = true;
                    hostBeat = *ppq;
                }
            }
        }
    }

    const bool preview = previewPlaying.load();
    const bool playing = hostPlaying || preview;
    const double samplesPerBeat = currentSampleRate * 60.0 / juce::jmax(1.0, bpm);
    // Host PPQ positions carry rounding error; continue from the previous block's end
    // when the host is still contiguous so boundary events aren't triggered twice.
    bool hostJumped = false;
    double startBeat = previewBeat;

    if (hostPlaying)
    {
        const double tolerance = 2.0 / samplesPerBeat;
        const bool contiguous = hostWasPlaying && std::abs(hostBeat - nextHostBeat) < tolerance;
        hostJumped = hostWasPlaying && !contiguous;
        startBeat = contiguous ? nextHostBeat : hostBeat;
    }

    const double endBeat = startBeat + numSamples / samplesPerBeat;

    // Live MIDI from the host keyboard/track also drives the preview synth.
    blockMidi.clear();
    blockMidi.addEvents(midiMessages, 0, numSamples, 0);

    {
        const juce::SpinLock::ScopedTryLockType lock(clipLock);

        if (lock.isLocked())
        {
            if (clipChanged || hostJumped || (wasPlaying && !playing))
            {
                sendAllNotesOff(blockMidi);
                clipChanged = false;
            }

            if (playing)
                collectClipEvents(startBeat, endBeat, samplesPerBeat, numSamples, blockMidi);

            playbackBeat.store(playing && clipEventsLength > 0.0
                                   ? std::fmod(juce::jmax(0.0, startBeat), clipEventsLength)
                                   : -1.0);

            if (preview && !hostPlaying && clipEventsLength > 0.0)
                previewBeat = std::fmod(endBeat, clipEventsLength);
        }
    }

    wasPlaying = playing;
    hostWasPlaying = hostPlaying;
    nextHostBeat = endBeat;

    simpleSynth.renderNextBlock(buffer, blockMidi, 0, numSamples);

    // Pass the same events on so the DAW can record/route them.
    midiMessages.swapWith(blockMidi);
}

void SynthesiaAIProcessor::collectClipEvents(double startBeat, double endBeat,
                                             double samplesPerBeat, int numSamples,
                                             juce::MidiBuffer& out) const
{
    if (clipEvents.empty() || clipEventsLength <= 0.0)
        return;

    // Events are assigned to their nearest sample, so shift the window by half a sample.
    const double halfSample = 0.5 / samplesPerBeat;
    const double windowStart = startBeat - halfSample;
    const double windowEnd = endBeat - halfSample;

    // Start one loop early so note-offs sitting exactly on the loop end are not dropped.
    const double firstLoop = (std::floor(windowStart / clipEventsLength) - 1.0) * clipEventsLength;

    for (double loopStart = firstLoop; loopStart < windowEnd; loopStart += clipEventsLength)
    {
        for (const auto& event : clipEvents)
        {
            const double beat = loopStart + event.beat;

            if (beat >= windowStart && beat < windowEnd)
            {
                const int samplePos = juce::jlimit(0, numSamples - 1,
                    static_cast<int>(std::floor((beat - startBeat) * samplesPerBeat + 0.5)));
                out.addEvent(event.message, samplePos);
            }
        }
    }
}

void SynthesiaAIProcessor::sendAllNotesOff(juce::MidiBuffer& out)
{
    for (int channel = 1; channel <= 16; ++channel)
        out.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
}

bool SynthesiaAIProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SynthesiaAIProcessor::createEditor()
{
    return new SynthesiaAIEditor(*this);
}

void SynthesiaAIProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::XmlElement xml("SynthesiaAIState");
    xml.setAttribute("generatorType", static_cast<int>(currentGeneratorType));
    xml.setAttribute("apiKey", apiKey);
    
    copyXmlToBinary(xml, destData);
}

void SynthesiaAIProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    
    if (xml != nullptr && xml->hasTagName("SynthesiaAIState"))
    {
        currentGeneratorType = static_cast<GeneratorType>(
            xml->getIntAttribute("generatorType", static_cast<int>(GeneratorType::Genre)));
        apiKey = xml->getStringAttribute("apiKey");
        
        setGeneratorType(currentGeneratorType);
    }
}

void SynthesiaAIProcessor::generateMidi(const MidiGenerationParams& params)
{
    if (isGeneratingMidi.load())
        return;
    
    isGeneratingMidi.store(true);

    bool merge = false;
    const auto prepared = prepareParams(params, merge);
    std::weak_ptr<bool> alive = aliveToken;
    
    std::thread([this, alive, prepared, merge, gen = generator]()
    {
        auto result = runGenerator(*gen, prepared);
        
        juce::MessageManager::callAsync([this, alive, prepared, merge, result = std::move(result)]() mutable
        {
            if (alive.expired())
                return;

            applyResult(std::move(result), prepared, merge);
            isGeneratingMidi.store(false);

            if (!generatedMidi.isEmpty())
                setPreviewPlaying(true);
        });
    }).detach();
}

void SynthesiaAIProcessor::generateMidiBlocking(const MidiGenerationParams& params)
{
    bool merge = false;
    const auto prepared = prepareParams(params, merge);
    applyResult(runGenerator(*generator, prepared), prepared, merge);
}

MidiGenerationParams SynthesiaAIProcessor::prepareParams(MidiGenerationParams params, bool& mergeIntoClip) const
{
    const bool sameContext = !progression.empty()
                          && params.rootNote == clipParams.rootNote
                          && params.scale == clipParams.scale
                          && params.genre == clipParams.genre
                          && params.lengthInBars == clipParams.lengthInBars;

    mergeIntoClip = params.part != Part::All && sameContext
                 && currentGeneratorType != GeneratorType::Markov;

    if (mergeIntoClip)
        params.progression = progression;
    else if (currentGeneratorType == GeneratorType::LLM)
        params.progression = ArrangementGenerator::buildProgression(params);

    return params;
}

SynthesiaAIProcessor::Result SynthesiaAIProcessor::runGenerator(MidiGenerator& gen,
                                                                 const MidiGenerationParams& params)
{
    if (auto* arranger = dynamic_cast<ArrangementGenerator*>(&gen))
    {
        auto arrangement = arranger->generateArrangement(params);
        return { std::move(arrangement.midi), std::move(arrangement.progression) };
    }

    return { gen.generate(params), params.progression };
}

void SynthesiaAIProcessor::applyResult(Result result, const MidiGenerationParams& params, bool mergeIntoClip)
{
    juce::MidiBuffer clip;

    if (mergeIntoClip)
    {
        for (const auto part : allParts)
            if (part != params.part)
                clip.addEvents(MidiExporter::filterPart(generatedMidi, part), 0, -1, 0);
        clip.addEvents(result.midi, 0, -1, 0);
    }
    else
    {
        clip = std::move(result.midi);
    }

    progression = std::move(result.progression);
    clipParams = params;
    clipParams.progression.clear();
    previewTempo.store(params.tempo);
    setGeneratedClip(std::move(clip), params.lengthInBars * 4.0);
}

void SynthesiaAIProcessor::setGeneratedClip(juce::MidiBuffer newMidi, double lengthInBeats)
{
    std::vector<ClipEvent> events;

    for (const auto metadata : newMidi)
        events.push_back({ metadata.samplePosition / ticksPerQuarterNote, metadata.getMessage() });

    generatedMidi = std::move(newMidi);
    clipLengthBeats = lengthInBeats;
    ++clipVersion;

    const juce::SpinLock::ScopedLockType lock(clipLock);
    clipEvents = std::move(events);
    clipEventsLength = lengthInBeats;
    clipChanged = true;
    previewBeat = 0.0;
}

void SynthesiaAIProcessor::setPreviewPlaying(bool shouldPlay)
{
    if (shouldPlay)
    {
        const juce::SpinLock::ScopedLockType lock(clipLock);
        previewBeat = 0.0;
    }

    previewPlaying.store(shouldPlay);
}

bool SynthesiaAIProcessor::isGenerating() const
{
    return isGeneratingMidi.load();
}

void SynthesiaAIProcessor::setGeneratorType(GeneratorType type)
{
    currentGeneratorType = type;
    
    switch (type)
    {
        case GeneratorType::Markov:
            generator = std::make_shared<MarkovGenerator>();
            break;
        case GeneratorType::LLM:
            generator = std::make_shared<LLMGenerator>(apiKey);
            break;
        case GeneratorType::Genre:
            generator = std::make_shared<ArrangementGenerator>();
            break;
    }
}

void SynthesiaAIProcessor::setApiKey(const juce::String& key)
{
    apiKey = key;
    if (currentGeneratorType == GeneratorType::LLM)
    {
        if (auto* llmGen = dynamic_cast<LLMGenerator*>(generator.get()))
        {
            llmGen->setApiKey(key);
        }
    }
}

juce::String SynthesiaAIProcessor::getApiKey() const
{
    return apiKey;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthesiaAIProcessor();
}
