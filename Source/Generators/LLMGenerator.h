#pragma once

#include "MidiGenerator.h"
#include "ArrangementPlan.h"
#include <juce_core/juce_core.h>
#include <optional>

class LLMGenerator : public MidiGenerator
{
public:
    explicit LLMGenerator(const juce::String& apiKey = "");
    ~LLMGenerator() override = default;

    // Notes are realised by ArrangementGenerator from plan(); this returns an empty buffer.
    juce::MidiBuffer generate(const MidiGenerationParams& params) override;

    ArrangementPlan plan(const MidiGenerationParams& params);

    void setApiKey(const juce::String& key) { apiKey = key; }
    juce::String getApiKey() const { return apiKey; }

    void setEndpoint(const juce::String& url);
    juce::String getEndpoint() const { return apiEndpoint; }

    void setModel(const juce::String& name);
    juce::String getModel() const { return model; }

    TokenUsage lastUsage() const { return lastTokens; }
    TokenUsage sessionUsage() const { return sessionTokens; }

private:
    struct ApiResult
    {
        juce::String content;
        TokenUsage usage;
    };

    juce::String buildPlanPrompt(const MidiGenerationParams& params) const;
    std::optional<ApiResult> callLLMAPI(const juce::String& prompt);

    juce::String apiKey;
    juce::String apiEndpoint{ "https://api.openai.com/v1/chat/completions" };
    juce::String model{ "gpt-4o-mini" };
    TokenUsage lastTokens;
    TokenUsage sessionTokens;
};
