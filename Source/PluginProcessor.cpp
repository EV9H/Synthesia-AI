#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Generators/MarkovGenerator.h"
#include "Generators/LLMGenerator.h"
#include "Generators/ArrangementGenerator.h"
#include "Generators/GenreProfiles.h"
#include "Music/MusicTheory.h"
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
    xml.setAttribute("apiEndpoint", apiEndpoint);
    xml.setAttribute("apiModel", apiModel);
    
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
        apiEndpoint = xml->getStringAttribute("apiEndpoint", apiEndpoint);
        apiModel = xml->getStringAttribute("apiModel", apiModel);
        
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
        configureLlm(*gen);
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

void SynthesiaAIProcessor::planArrangement(const MidiGenerationParams& params)
{
    if (isGeneratingMidi.load())
        return;

    isGeneratingMidi.store(true);
    bool merge = false;
    const auto prepared = prepareParams(params, merge);
    std::weak_ptr<bool> alive = aliveToken;

    std::thread([this, alive, prepared, gen = generator]()
    {
        configureLlm(*gen);
        auto planned = createPlan(prepared);

        juce::MessageManager::callAsync([this, alive, planned = std::move(planned)]() mutable
        {
            if (alive.expired())
                return;

            storePlan(std::move(planned), true);
            isGeneratingMidi.store(false);
        });
    }).detach();
}

ArrangementPlan SynthesiaAIProcessor::planArrangementBlocking(const MidiGenerationParams& params)
{
    bool merge = false;
    const auto prepared = prepareParams(params, merge);
    configureLlm(*generator);
    auto planned = createPlan(prepared);
    storePlan(planned, true);
    return planned;
}

MidiGenerationParams SynthesiaAIProcessor::prepareParams(MidiGenerationParams params, bool& mergeIntoClip) const
{
    const bool sameContext = !progression.empty()
                          && params.rootNote == clipParams.rootNote
                          && params.scale == clipParams.scale
                          && params.genre == clipParams.genre
                          && params.lengthInBars == clipParams.lengthInBars
                          && params.form == clipParams.form;

    const bool keepParts = params.part != Part::All || params.lockedParts != 0;
    mergeIntoClip = keepParts && sameContext
                 && currentGeneratorType != GeneratorType::Markov;

    if (params.lockProgression && !progression.empty() && sameContext)
        params.progression = progression;

    if (!params.planJson.empty() && params.plannedSections.empty())
    {
        const auto seed = params.seed;
        const auto part = params.part;
        const auto locked = params.lockedParts;
        const auto quant = params.quantization;
        const auto style = params.stylePrompt;
        const auto chordOverride = params.chordText;
        auto plan = ArrangementPlan::parseJson(params.planJson, params);
        plan.applyTo(params);
        params.seed = seed;
        params.part = part;
        params.lockedParts = locked;
        params.quantization = quant;
        params.stylePrompt = style;
        if (!chordOverride.empty())
        {
            params.chordText = chordOverride;
            params.progression.clear();
            params.lockProgression = true;
        }
    }

    if (params.progression.empty() && !params.chordText.empty())
    {
        const auto& profile = getGenreProfile(params.genre);
        params.progression = theory::parseProgressionText(params.chordText, params.rootNote,
                                                          params.scale, params.lengthInBars,
                                                          profile.defaultExtension);
    }

    if (mergeIntoClip && params.progression.empty())
        params.progression = progression;

    return params;
}

void SynthesiaAIProcessor::configureLlm(MidiGenerator& gen) const
{
    if (auto* llm = dynamic_cast<LLMGenerator*>(&gen))
    {
        llm->setApiKey(apiKey);
        llm->setEndpoint(apiEndpoint);
        llm->setModel(apiModel);
    }
}

ArrangementPlan SynthesiaAIProcessor::createPlan(const MidiGenerationParams& params)
{
    if (auto* llm = dynamic_cast<LLMGenerator*>(generator.get()))
        return llm->plan(params);

    if (!params.planJson.empty())
        return ArrangementPlan::parseJson(params.planJson, params);

    return ArrangementPlan::fromKeywords(params.stylePrompt, params);
}

void SynthesiaAIProcessor::storePlan(ArrangementPlan plan, bool bumpUi)
{
    lastPlan = std::move(plan);
    if (auto* llm = dynamic_cast<LLMGenerator*>(generator.get()))
        sessionUsage = llm->sessionUsage();
    if (bumpUi)
        ++planVersion;
}

SynthesiaAIProcessor::Result SynthesiaAIProcessor::runGenerator(MidiGenerator& gen,
                                                                 const MidiGenerationParams& params)
{
    configureLlm(gen);

    if (auto* llm = dynamic_cast<LLMGenerator*>(&gen))
    {
        ArrangementPlan planned;
        const bool reuse = lastPlan.sections.size() > 0
                        && lastPlan.prompt == params.stylePrompt
                        && (lastPlan.source == "llm" || lastPlan.source == "heuristic" || lastPlan.source == "json");
        if (!params.planJson.empty())
            planned = ArrangementPlan::parseJson(params.planJson, params);
        else if (reuse)
            planned = lastPlan;
        else
            planned = llm->plan(params);

        MidiGenerationParams realised = params;
        planned.applyTo(realised);
        realised.seed = params.seed;
        realised.part = params.part;
        realised.lockedParts = params.lockedParts;
        realised.quantization = params.quantization;
        if (!params.chordText.empty())
        {
            realised.chordText = params.chordText;
            realised.progression.clear();
            realised.lockProgression = true;
        }

        ArrangementGenerator arranger;
        auto arrangement = arranger.generateArrangement(realised);
        Result result;
        result.midi = std::move(arrangement.midi);
        result.progression = std::move(arrangement.progression);
        result.sections = std::move(arrangement.sections);
        result.plan = std::move(planned);
        result.hasPlan = true;
        result.realised = realised;
        return result;
    }

    if (auto* arranger = dynamic_cast<ArrangementGenerator*>(&gen))
    {
        auto arrangement = arranger->generateArrangement(params);
        Result result;
        result.midi = std::move(arrangement.midi);
        result.progression = std::move(arrangement.progression);
        result.sections = std::move(arrangement.sections);
        result.realised = params;
        if (!params.planJson.empty())
        {
            result.hasPlan = true;
            result.plan = ArrangementPlan::parseJson(params.planJson, params);
        }
        return result;
    }

    Result result;
    result.midi = gen.generate(params);
    result.progression = params.progression;
    result.realised = params;
    return result;
}

void SynthesiaAIProcessor::applyResult(Result result, const MidiGenerationParams& params, bool mergeIntoClip)
{
    if (result.hasPlan)
        storePlan(result.plan, planVersion == 0);

    if (!result.midiReady)
        return;

    juce::MidiBuffer clip;

    if (mergeIntoClip)
    {
        for (const auto part : allParts)
        {
            const bool keep = (params.part != Part::All && part != params.part)
                           || isPartLocked(params.lockedParts, part);
            if (keep)
                clip.addEvents(MidiExporter::filterPart(generatedMidi, part), 0, -1, 0);
            else
                clip.addEvents(MidiExporter::filterPart(result.midi, part), 0, -1, 0);
        }
    }
    else
    {
        clip = std::move(result.midi);
    }

    const auto& used = result.realised.lengthInBars > 0 ? result.realised : params;

    if (!(params.lockProgression && !progression.empty()) || result.progression.empty())
        progression = std::move(result.progression);
    if (!result.sections.empty())
        sections = std::move(result.sections);
    clipParams = used;
    clipParams.progression.clear();
    previewTempo.store(used.tempo);
    simpleSynth.setKit(used.genre);
    setGeneratedClip(std::move(clip), used.lengthInBars * 4.0);
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
            configureLlm(*generator);
            break;
        case GeneratorType::Genre:
            generator = std::make_shared<ArrangementGenerator>();
            break;
    }
}

void SynthesiaAIProcessor::setApiKey(const juce::String& key)
{
    apiKey = key;
    configureLlm(*generator);
}

juce::String SynthesiaAIProcessor::getApiKey() const
{
    return apiKey;
}

void SynthesiaAIProcessor::setApiEndpoint(const juce::String& url)
{
    const auto trimmed = url.trim();
    if (trimmed.isNotEmpty())
        apiEndpoint = trimmed;
    configureLlm(*generator);
}

void SynthesiaAIProcessor::setApiModel(const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isNotEmpty())
        apiModel = trimmed;
    configureLlm(*generator);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthesiaAIProcessor();
}
