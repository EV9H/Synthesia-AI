#include "LLMGenerator.h"

namespace
{
    double asNumberSafe(const juce::var& value)
    {
        if (value.isVoid() || value.isUndefined())
            return 0.0;
        return static_cast<double>(value);
    }
}

LLMGenerator::LLMGenerator(const juce::String& key)
    : apiKey(key)
{
}

void LLMGenerator::setEndpoint(const juce::String& url)
{
    const auto trimmed = url.trim();
    if (trimmed.isNotEmpty())
        apiEndpoint = trimmed;
}

void LLMGenerator::setModel(const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isNotEmpty())
        model = trimmed;
}

juce::MidiBuffer LLMGenerator::generate(const MidiGenerationParams& params)
{
    juce::ignoreUnused(params);
    return {};
}

ArrangementPlan LLMGenerator::plan(const MidiGenerationParams& params)
{
    lastTokens = {};

    if (apiKey.isEmpty())
        return ArrangementPlan::fromKeywords(params.stylePrompt, params);

    auto response = callLLMAPI(buildPlanPrompt(params));
    if (!response.has_value() || response->content.isEmpty())
        return ArrangementPlan::fromKeywords(params.stylePrompt, params);

    auto planned = ArrangementPlan::parseJson(response->content, params);
    planned.source = "llm";
    planned.prompt = params.stylePrompt;
    planned.usage = response->usage;
    lastTokens = response->usage;
    sessionTokens.promptTokens += response->usage.promptTokens;
    sessionTokens.completionTokens += response->usage.completionTokens;
    sessionTokens.totalTokens += response->usage.totalTokens;
    ++sessionTokens.requests;
    return planned;
}

juce::String LLMGenerator::buildPlanPrompt(const MidiGenerationParams& params) const
{
    static const char* scaleNames[] = { "major", "minor", "dorian", "mixolydian", "pentatonic" };
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String prompt;
    prompt << "Write an arrangement plan for this brief:\n"
           << juce::String(params.stylePrompt.empty() ? "instrumental pop song" : params.stylePrompt)
           << "\n\nHints (override them if the brief disagrees):\n"
           << "- Key: " << noteNames[juce::jlimit(0, 11, params.rootNote)] << "\n"
           << "- Scale: " << scaleNames[static_cast<int>(params.scale)] << "\n"
           << "- Genre: " << genreName(params.genre) << "\n"
           << "- Tempo: " << params.tempo << " BPM\n"
           << "- Length: " << params.lengthInBars << " bars of 4/4\n";
    if (!params.chordText.empty())
        prompt << "- Honour this chord progression: " << juce::String(params.chordText) << "\n";
    prompt << "\n"
           << ArrangementPlan::schemaPrompt() << "\n"
           << "Keep the form to 8-32 bars. Put the chorus/drop energy high if the brief asks for a lift.";
    return prompt;
}

std::optional<LLMGenerator::ApiResult> LLMGenerator::callLLMAPI(const juce::String& prompt)
{
    juce::DynamicObject::Ptr requestObj = new juce::DynamicObject();
    requestObj->setProperty("model", model);
    requestObj->setProperty("temperature", 0.4);

    juce::Array<juce::var> messages;

    juce::DynamicObject::Ptr system = new juce::DynamicObject();
    system->setProperty("role", "system");
    system->setProperty("content",
                        "You are a music arranger. Output ONLY a JSON object matching the given schema. "
                        "Never emit MIDI notes, pitches, or markdown.");
    messages.add(juce::var(system.get()));

    juce::DynamicObject::Ptr user = new juce::DynamicObject();
    user->setProperty("role", "user");
    user->setProperty("content", prompt);
    messages.add(juce::var(user.get()));

    requestObj->setProperty("messages", messages);

    const juce::String requestBody = juce::JSON::toString(juce::var(requestObj.get()));
    juce::URL url = juce::URL(apiEndpoint).withPOSTData(requestBody);
    const juce::String extraHeaders = "Content-Type: application/json\r\n"
                                      "Authorization: Bearer " + apiKey + "\r\n";

    std::unique_ptr<juce::InputStream> stream(url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders(extraHeaders)
            .withConnectionTimeoutMs(30000)
            .withNumRedirectsToFollow(0)));

    if (stream == nullptr)
    {
        DBG("Failed to create input stream for API request");
        return std::nullopt;
    }

    const juce::String response = stream->readEntireStreamAsString();
    juce::var parsedResponse;
    if (!juce::JSON::parse(response, parsedResponse).wasOk())
    {
        DBG("Failed to parse API response");
        return std::nullopt;
    }

    auto* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr)
        return std::nullopt;

    ApiResult result;
    if (auto* usage = obj->getProperty("usage").getDynamicObject())
    {
        result.usage.promptTokens = juce::roundToInt(asNumberSafe(usage->getProperty("prompt_tokens")));
        result.usage.completionTokens = juce::roundToInt(asNumberSafe(usage->getProperty("completion_tokens")));
        result.usage.totalTokens = juce::roundToInt(asNumberSafe(usage->getProperty("total_tokens")));
        if (result.usage.totalTokens <= 0)
            result.usage.totalTokens = result.usage.promptTokens + result.usage.completionTokens;
        result.usage.requests = 1;
    }

    if (auto* choices = obj->getProperty("choices").getArray())
    {
        if (choices->size() > 0)
        {
            if (auto* choice = choices->getUnchecked(0).getDynamicObject())
            {
                if (auto* message = choice->getProperty("message").getDynamicObject())
                {
                    result.content = message->getProperty("content").toString();
                    return result;
                }
            }
        }
    }

    DBG("Failed to extract content from API response");
    return std::nullopt;
}
