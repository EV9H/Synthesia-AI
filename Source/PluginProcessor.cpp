#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Generators/MarkovGenerator.h"
#include "Generators/LLMGenerator.h"
#include "Generators/ArrangementGenerator.h"
#include "Generators/GenreProfiles.h"
#include "Music/MusicTheory.h"
#include "MidiExporter.h"
#include "StyleProfile.h"
#include "PresetStore.h"
#include "LicenseStub.h"
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
    lastUiParams = PresetStore::factoryByIndex(0).params;
    currentPresetName = PresetStore::factoryByIndex(0).name;
    loadLocalSettings();
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
    return PresetStore::factoryPresets().size();
}

int SynthesiaAIProcessor::getCurrentProgram()
{
    return currentProgram;
}

void SynthesiaAIProcessor::setCurrentProgram(int index)
{
    const auto factories = PresetStore::factoryPresets();
    if (index < 0 || index >= factories.size())
        return;
    currentProgram = index;
    applyPreset(factories.getReference(index).id);
}

const juce::String SynthesiaAIProcessor::getProgramName(int index)
{
    const auto factories = PresetStore::factoryPresets();
    if (index < 0 || index >= factories.size())
        return {};
    return factories.getReference(index).name;
}

void SynthesiaAIProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void SynthesiaAIProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    simpleSynth.prepare(sampleRate, samplesPerBlock);
    simpleSynth.setTempo(previewTempo.load());
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

    simpleSynth.setTempo(bpm);

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

    if (captureEnabled.load())
    {
        const juce::SpinLock::ScopedLockType lock(captureLock);
        if (!captureHasOrigin)
        {
            captureOriginBeat = startBeat;
            captureHasOrigin = true;
        }
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn() || message.isNoteOff())
            {
                const double beat = startBeat + metadata.samplePosition / samplesPerBeat;
                captureEvents.push_back({ beat, message });
            }
        }
    }

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
                if (event.message.getChannel() > 0
                    && !isPartAudible(partFromChannel(event.message.getChannel())))
                    continue;

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
    xml.setAttribute("version", 2);
    xml.setAttribute("generatorType", static_cast<int>(currentGeneratorType));
    xml.setAttribute("apiEndpoint", apiEndpoint);
    xml.setAttribute("apiModel", apiModel);
    xml.setAttribute("apiProvider", apiProvider);
    xml.setAttribute("styleName", currentStyleName);
    xml.setAttribute("styleBlend", styleBlend);
    xml.setAttribute("presetName", currentPresetName);
    xml.setAttribute("program", currentProgram);
    xml.setAttribute("muteMask", muteMask.load());
    xml.setAttribute("soloMask", soloMask.load());
    xml.setAttribute("licenseKey", licenseKey);
    xml.setAttribute("clipTempo", previewTempo.load());
    xml.setAttribute("clipLengthBeats", clipLengthBeats);
    xml.setAttribute("clipMidi", encodeClipMidi());

    auto* paramsXml = xml.createNewChildElement("UiParams");
    PresetStore::writeParams(*paramsXml, lastUiParams);
    simpleSynth.writeSoundXml(*xml.createNewChildElement("PreviewSound"));

    if (!currentStyle.empty())
        xml.setAttribute("styleJson", currentStyle.toJson());

    copyXmlToBinary(xml, destData);
}

void SynthesiaAIProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    
    if (xml == nullptr || !xml->hasTagName("SynthesiaAIState"))
        return;

    currentGeneratorType = static_cast<GeneratorType>(
        xml->getIntAttribute("generatorType", static_cast<int>(GeneratorType::Genre)));
    apiEndpoint = xml->getStringAttribute("apiEndpoint", apiEndpoint);
    apiModel = xml->getStringAttribute("apiModel", apiModel);
    apiProvider = xml->getStringAttribute("apiProvider", apiProvider);
    setStyleBlend(xml->getDoubleAttribute("styleBlend", styleBlend));
    const auto styleName = xml->getStringAttribute("styleName");
    const auto styleJson = xml->getStringAttribute("styleJson");
    if (styleJson.isNotEmpty())
    {
        currentStyle = StyleProfile::fromJson(styleJson);
        currentStyleName = styleName.isNotEmpty() ? styleName : juce::String(currentStyle.name);
        ++styleVersion;
    }
    else
    {
        setCurrentStyle(styleName);
    }

    currentPresetName = xml->getStringAttribute("presetName", currentPresetName);
    currentProgram = xml->getIntAttribute("program", currentProgram);
    muteMask.store(xml->getIntAttribute("muteMask", 0));
    soloMask.store(xml->getIntAttribute("soloMask", 0));
    licenseKey = xml->getStringAttribute("licenseKey", licenseKey);

    if (auto* paramsXml = xml->getChildByName("UiParams"))
        setLastUiParams(PresetStore::readParams(*paramsXml));
    if (auto* soundXml = xml->getChildByName("PreviewSound"))
        simpleSynth.readSoundXml(*soundXml);
    else
        simpleSynth.loadGenreDefaults(lastUiParams.genre);

    const auto clipB64 = xml->getStringAttribute("clipMidi");
    if (clipB64.isNotEmpty())
        restoreClipMidi(clipB64);

    const auto savedTempo = xml->getDoubleAttribute("clipTempo", previewTempo.load());
    if (savedTempo > 0.0)
        previewTempo.store(savedTempo);

    setGeneratorType(currentGeneratorType);
    persistLocalSettings();
}

void SynthesiaAIProcessor::generateMidi(const MidiGenerationParams& params)
{
    if (isGeneratingMidi.load())
        return;

    setLastUiParams(params);
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
    setLastUiParams(params);
    bool merge = false;
    const auto prepared = prepareParams(params, merge);
    applyResult(runGenerator(*generator, prepared), prepared, merge);
    setPreviewPlaying(false);
}

void SynthesiaAIProcessor::planArrangement(const MidiGenerationParams& params)
{
    if (isGeneratingMidi.load())
        return;

    setLastUiParams(params);
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
                 && currentGeneratorType != GeneratorType::Markov
                 && params.importMode == ImportMode::None;

    if (params.styleProfileJson.empty() && !currentStyle.empty())
    {
        params.styleProfileJson = currentStyle.toJson().toStdString();
        params.styleProfileName = currentStyleName.toStdString();
        if (params.styleBlend <= 0.0)
            params.styleBlend = styleBlend;
    }

    if (lastImport.ok && params.importMode != ImportMode::None)
    {
        if (params.chordText.empty() && !lastImport.chordChart.empty())
        {
            params.chordText = lastImport.chordChart;
            params.lockProgression = true;
        }
        if (params.motifContour.empty())
            params.motifContour = lastImport.motifContour;
        if (params.motifRhythm.empty())
            params.motifRhythm = lastImport.motifRhythm;
        if (params.importMode == ImportMode::Accompany || params.importMode == ImportMode::Extend
            || params.importMode == ImportMode::Restyle)
            params.lockedParts |= partLockBit(lastImport.primaryPart);
        if (params.importMode == ImportMode::Extend && params.form == ArrangementForm::Loop
            && params.lengthInBars >= 16)
            params.form = ArrangementForm::Song;
    }

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
        const auto profile = StyleProfile::effectiveGenre(params);
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

    if (params.importMode != ImportMode::None && lastImport.ok)
        clip = MidiImporter::overlay(clip, lastImport, params.importMode, params.lockedParts,
                                     used.lengthInBars * 4.0);

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
    persistLocalSettings();
}

juce::String SynthesiaAIProcessor::getApiKey() const
{
    return apiKey;
}

void SynthesiaAIProcessor::setApiEndpoint(const juce::String& url)
{
    const auto trimmed = completeChatUrl(url);
    if (trimmed.isNotEmpty())
        apiEndpoint = trimmed;
    configureLlm(*generator);
    persistLocalSettings();
}

void SynthesiaAIProcessor::applyImport(MidiAnalysis analysis)
{
    lastImport = std::move(analysis);
    ++importVersion;
    if (!lastImport.ok)
        return;

    progression = lastImport.progression;
    sections = { { SectionKind::Verse, 0.0, lastImport.lengthInBeats, 0.7f, 1, "Imported" } };
    clipParams.rootNote = lastImport.rootNote;
    clipParams.scale = lastImport.scale;
    clipParams.tempo = lastImport.tempo;
    clipParams.lengthInBars = lastImport.lengthInBars;
    clipParams.chordText = lastImport.chordChart;
    clipParams.lockProgression = true;
    clipParams.genre = clipParams.genre;
    previewTempo.store(lastImport.tempo);
    simpleSynth.setKit(clipParams.genre);
    setGeneratedClip(lastImport.remapped, lastImport.lengthInBeats);
    setPreviewPlaying(true);
}

bool SynthesiaAIProcessor::importMidiFile(const juce::File& file)
{
    applyImport(MidiImporter::analyseFile(file));
    return lastImport.ok;
}

bool SynthesiaAIProcessor::importMidiBuffer(const juce::MidiBuffer& midi, double ticksPerQuarter, double tempo,
                                            const juce::String& sourceName)
{
    applyImport(MidiImporter::analyseBuffer(midi, ticksPerQuarter, tempo, sourceName));
    return lastImport.ok;
}

void SynthesiaAIProcessor::setMidiCapture(bool shouldCapture)
{
    if (shouldCapture)
    {
        const juce::SpinLock::ScopedLockType lock(captureLock);
        captureEvents.clear();
        captureHasOrigin = false;
        captureOriginBeat = 0.0;
        captureEnabled.store(true);
        return;
    }

    captureEnabled.store(false);
}

bool SynthesiaAIProcessor::finishMidiCapture()
{
    captureEnabled.store(false);
    juce::MidiBuffer captured;
    {
        const juce::SpinLock::ScopedLockType lock(captureLock);
        if (captureEvents.empty())
            return false;

        const double origin = captureHasOrigin ? captureOriginBeat : captureEvents.front().beat;
        for (const auto& event : captureEvents)
        {
            const int tick = juce::roundToInt((event.beat - origin) * ticksPerQuarterNote);
            captured.addEvent(event.message, juce::jmax(0, tick));
        }
        captureEvents.clear();
        captureHasOrigin = false;
    }

    const double tempo = previewTempo.load();
    return importMidiBuffer(captured, ticksPerQuarterNote, tempo, "host capture");
}

juce::String SynthesiaAIProcessor::getCurrentStyleJson() const
{
    return currentStyle.empty() ? juce::String{} : currentStyle.toJson();
}

void SynthesiaAIProcessor::setStyleBlend(double amount)
{
    styleBlend = juce::jlimit(0.0, 1.0, amount);
}

void SynthesiaAIProcessor::setCurrentStyle(const juce::String& name)
{
    currentStyleName = StyleProfile::sanitiseName(name);
    if (name.trim().isEmpty() || name == "None")
    {
        currentStyle = {};
        currentStyleName = {};
        ++styleVersion;
        return;
    }

    currentStyle = StyleProfile::loadNamed(currentStyleName);
    if (currentStyle.empty())
        currentStyleName = {};
    ++styleVersion;
}

bool SynthesiaAIProcessor::learnStyle(const juce::Array<juce::File>& files, const juce::String& name)
{
    auto learned = StyleProfile::learnFromFiles(files);
    if (learned.empty())
        return false;

    learned.name = StyleProfile::sanitiseName(name).toStdString();
    if (learned.name.empty())
        learned.name = "style";
    learned.finaliseSummary();
    if (!StyleProfile::saveNamed(learned))
        return false;

    currentStyle = std::move(learned);
    currentStyleName = juce::String(currentStyle.name);
    if (styleBlend < 0.25)
        styleBlend = 1.0;
    ++styleVersion;
    return true;
}

void SynthesiaAIProcessor::setApiModel(const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isNotEmpty())
        apiModel = trimmed;
    configureLlm(*generator);
    persistLocalSettings();
}

void SynthesiaAIProcessor::setLastUiParams(const MidiGenerationParams& params)
{
    lastUiParams = params;
    ++uiParamsVersion;
}

void SynthesiaAIProcessor::setPreviewPatch(Part part, const SynthPatch& patch)
{
    simpleSynth.setPatch(part, patch);
}

SynthPatch SynthesiaAIProcessor::getPreviewPatch(Part part) const
{
    return simpleSynth.getPatch(part);
}

void SynthesiaAIProcessor::setPreviewMix(const MixSettings& settings)
{
    simpleSynth.setMix(settings);
}

MixSettings SynthesiaAIProcessor::getPreviewMix() const
{
    return simpleSynth.getMix();
}

void SynthesiaAIProcessor::loadGenreSounds(Genre genre)
{
    simpleSynth.loadGenreDefaults(genre);
}

bool SynthesiaAIProcessor::applyPreset(const juce::String& idOrName)
{
    auto found = PresetStore::find(idOrName);
    if (!found)
        return false;

    currentPresetName = found->name;
    const auto factories = PresetStore::factoryPresets();
    currentProgram = 0;
    for (int i = 0; i < factories.size(); ++i)
        if (factories.getReference(i).id == found->id)
            currentProgram = i;

    setGeneratorType(found->generator);
    if (!found->params.styleProfileName.empty())
        setCurrentStyle(juce::String(found->params.styleProfileName));
    setStyleBlend(found->params.styleBlend);
    setLastUiParams(found->params);
    simpleSynth.loadGenreDefaults(found->params.genre);
    ++presetVersion;
    return true;
}

bool SynthesiaAIProcessor::saveUserPreset(const juce::String& name)
{
    if (!PresetStore::saveUser(name, lastUiParams, currentGeneratorType))
        return false;
    currentPresetName = name;
    ++presetVersion;
    return true;
}

void SynthesiaAIProcessor::setPartMuted(Part part, bool shouldMute)
{
    int mask = muteMask.load();
    const int bit = partLockBit(part);
    mask = shouldMute ? (mask | bit) : (mask & ~bit);
    muteMask.store(mask);
}

void SynthesiaAIProcessor::setPartSoloed(Part part, bool shouldSolo)
{
    int mask = soloMask.load();
    const int bit = partLockBit(part);
    mask = shouldSolo ? (mask | bit) : (mask & ~bit);
    soloMask.store(mask);
}

bool SynthesiaAIProcessor::isPartMuted(Part part) const
{
    return (muteMask.load() & partLockBit(part)) != 0;
}

bool SynthesiaAIProcessor::isPartSoloed(Part part) const
{
    return (soloMask.load() & partLockBit(part)) != 0;
}

bool SynthesiaAIProcessor::isPartAudible(Part part) const
{
    const int solo = soloMask.load();
    if (solo != 0)
        return (solo & partLockBit(part)) != 0;
    return (muteMask.load() & partLockBit(part)) == 0;
}

juce::File SynthesiaAIProcessor::writeTempMidi(Part part) const
{
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile("SynthesiaAI-drag");
    dir.createDirectory();
    const auto stem = juce::String(partName(part)).replaceCharacters(" /", "--").replace("--", "-");
    auto file = dir.getChildFile(stem + ".mid");
    const auto midi = MidiExporter::filterPart(generatedMidi, part);
    MidiExporter::exportToFile(midi, file, previewTempo.load());
    return file;
}

void SynthesiaAIProcessor::setLicenseKey(const juce::String& key)
{
    licenseKey = key.trim();
    persistLocalSettings();
}

void SynthesiaAIProcessor::setApiProvider(const juce::String& name)
{
    apiProvider = name.trim();
    if (apiProvider.equalsIgnoreCase("Groq"))
    {
        apiEndpoint = groqChatUrl();
        apiModel = groqDefaultModel();
    }
    else if (apiProvider.equalsIgnoreCase("OpenAI"))
    {
        apiEndpoint = openaiChatUrl();
        apiModel = "gpt-4o-mini";
    }
    configureLlm(*generator);
    persistLocalSettings();
}

juce::String SynthesiaAIProcessor::completeChatUrl(juce::String url)
{
    url = url.trim();
    if (url.endsWithChar('/'))
        url = url.dropLastCharacters(1);
    if (url.isNotEmpty() && !url.containsIgnoreCase("chat/completions"))
        url += "/chat/completions";
    return url;
}

juce::File SynthesiaAIProcessor::localSettingsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("SynthesiaAI");
    dir.createDirectory();
    return dir.getChildFile("settings.xml");
}

void SynthesiaAIProcessor::persistLocalSettings() const
{
    juce::XmlElement xml("SynthesiaAILocal");
    xml.setAttribute("apiKey", apiKey);
    xml.setAttribute("apiEndpoint", apiEndpoint);
    xml.setAttribute("apiModel", apiModel);
    xml.setAttribute("apiProvider", apiProvider);
    xml.setAttribute("licenseKey", licenseKey);
    xml.writeTo(localSettingsFile());
}

void SynthesiaAIProcessor::loadLocalSettings()
{
    const auto file = localSettingsFile();
    if (!file.existsAsFile())
        return;
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("SynthesiaAILocal"))
        return;
    apiKey = xml->getStringAttribute("apiKey");
    apiEndpoint = xml->getStringAttribute("apiEndpoint", apiEndpoint);
    apiModel = xml->getStringAttribute("apiModel", apiModel);
    apiProvider = xml->getStringAttribute("apiProvider", apiProvider);
    licenseKey = xml->getStringAttribute("licenseKey");
    configureLlm(*generator);
}

juce::String SynthesiaAIProcessor::encodeClipMidi() const
{
    if (generatedMidi.isEmpty())
        return {};

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote(960);
    juce::MidiMessageSequence track;
    for (const auto metadata : generatedMidi)
        track.addEvent(metadata.getMessage(), 0.0);
    midiFile.addTrack(track);

    juce::MemoryOutputStream stream;
    if (!midiFile.writeTo(stream))
        return {};
    return juce::Base64::toBase64(stream.getData(), stream.getDataSize());
}

bool SynthesiaAIProcessor::restoreClipMidi(const juce::String& base64)
{
    juce::MemoryOutputStream decoded;
    if (!juce::Base64::convertFromBase64(decoded, base64))
        return false;

    juce::MemoryInputStream input(decoded.getData(), decoded.getDataSize(), false);
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(input))
        return false;

    juce::MidiBuffer restored;
    const int ticks = midiFile.getTimeFormat() > 0 ? midiFile.getTimeFormat() : 960;
    const double scale = 960.0 / static_cast<double>(ticks);
    double lastTick = 0.0;
    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        if (auto* track = midiFile.getTrack(t))
        {
            for (int i = 0; i < track->getNumEvents(); ++i)
            {
                const auto* held = track->getEventPointer(i);
                const auto message = held->message;
                if (message.isMetaEvent())
                    continue;
                const int tick = juce::roundToInt(message.getTimeStamp() * scale);
                restored.addEvent(message, tick);
                lastTick = juce::jmax(lastTick, static_cast<double>(tick));
            }
        }
    }

    const double length = clipLengthBeats > 0.0 ? clipLengthBeats
                                                : juce::jmax(4.0, lastTick / 960.0);
    simpleSynth.setKit(lastUiParams.genre);
    clipParams = lastUiParams;
    setGeneratedClip(std::move(restored), length);
    setPreviewPlaying(false);
    return true;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SynthesiaAIProcessor();
}
