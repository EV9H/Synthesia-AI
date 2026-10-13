#pragma once

#include "MidiGenerator.h"
#include <juce_core/juce_core.h>

class LLMGenerator : public MidiGenerator
{
public:
    explicit LLMGenerator(const juce::String& apiKey = "");
    ~LLMGenerator() override = default;
    
    juce::MidiBuffer generate(const MidiGenerationParams& params) override;
    
    void setApiKey(const juce::String& key) { apiKey = key; }
    juce::String getApiKey() const { return apiKey; }

private:
    juce::String apiKey;
    juce::String apiEndpoint{"https://api.openai.com/v1/chat/completions"};
    
    std::vector<Note> parseNotesFromJSON(const juce::String& jsonResponse,
                                         const MidiGenerationParams& params,
                                         std::vector<int>* channels = nullptr);
    juce::String buildPrompt(const MidiGenerationParams& params);
    std::optional<juce::String> callLLMAPI(const juce::String& prompt);
};
