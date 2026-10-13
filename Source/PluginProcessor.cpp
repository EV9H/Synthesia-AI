#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Generators/MarkovGenerator.h"
#include "Generators/LLMGenerator.h"
#include <thread>

SynthesiaAIProcessor::SynthesiaAIProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    generator = std::make_unique<MarkovGenerator>();
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
    return 0.0;
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
    simpleSynth.prepare(sampleRate, samplesPerBlock);
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
    
    // Process generated MIDI through simple synth for preview
    if (!generatedMidi.isEmpty())
    {
        simpleSynth.renderNextBlock(buffer, generatedMidi, 0, buffer.getNumSamples());
    }
    
    // Also output MIDI (for DAW routing)
    midiMessages.addEvents(generatedMidi, 0, buffer.getNumSamples(), 0);
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
            xml->getIntAttribute("generatorType", static_cast<int>(GeneratorType::Markov)));
        apiKey = xml->getStringAttribute("apiKey");
        
        setGeneratorType(currentGeneratorType);
    }
}

void SynthesiaAIProcessor::generateMidi(const MidiGenerationParams& params)
{
    if (isGeneratingMidi.load())
        return;
    
    isGeneratingMidi.store(true);
    
    // Run generation on background thread using async
    std::thread([this, params]()
    {
        auto newMidi = generator->generate(params);
        
        // Update generated MIDI on message thread
        juce::MessageManager::callAsync([this, newMidi = std::move(newMidi)]() mutable
        {
            generatedMidi = std::move(newMidi);
            isGeneratingMidi.store(false);
        });
    }).detach();
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
            generator = std::make_unique<MarkovGenerator>();
            break;
        case GeneratorType::LLM:
            generator = std::make_unique<LLMGenerator>(apiKey);
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
