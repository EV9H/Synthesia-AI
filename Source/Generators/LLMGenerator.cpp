#include "LLMGenerator.h"
#include <juce_core/juce_core.h>

LLMGenerator::LLMGenerator(const juce::String& key)
    : apiKey(key)
{
}

juce::MidiBuffer LLMGenerator::generate(const MidiGenerationParams& params)
{
    if (apiKey.isEmpty())
    {
        DBG("API key not set - returning empty buffer");
        return juce::MidiBuffer();
    }
    
    auto prompt = buildPrompt(params);
    auto response = callLLMAPI(prompt);
    
    if (!response.has_value())
    {
        DBG("LLM API call failed");
        return juce::MidiBuffer();
    }
    
    auto notes = parseNotesFromJSON(*response, params);
    return notesToMidiBuffer(notes, params.tempo);
}

juce::String LLMGenerator::buildPrompt(const MidiGenerationParams& params)
{
    juce::String scaleNames[] = {"Major", "Minor", "Dorian", "Mixolydian", "Pentatonic"};
    juce::String noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    juce::String prompt;
    prompt << "Generate a MIDI melody with the following specifications:\n"
           << "- Key: " << noteNames[params.rootNote] << " " << scaleNames[static_cast<int>(params.scale)] << "\n"
           << "- Length: " << params.lengthInBars << " bars (4/4 time)\n"
           << "- Style: " << juce::String(params.stylePrompt) << "\n\n"
           << "Return ONLY a JSON array of note objects with this exact format:\n"
           << "[{\"pitch\": 60, \"start\": 0.0, \"duration\": 1.0, \"velocity\": 80}, ...]\n"
           << "where:\n"
           << "- pitch: MIDI note number (0-127)\n"
           << "- start: start time in quarter notes from beginning\n"
           << "- duration: note duration in quarter notes\n"
           << "- velocity: note velocity (60-100)\n\n"
           << "Total length must be exactly " << (params.lengthInBars * 4) << " quarter notes.";
    
    return prompt;
}

std::optional<juce::String> LLMGenerator::callLLMAPI(const juce::String& prompt)
{
    // Build request JSON
    juce::DynamicObject::Ptr requestObj = new juce::DynamicObject();
    requestObj->setProperty("model", "gpt-4");
    requestObj->setProperty("temperature", 0.7);
    
    juce::Array<juce::var> messages;
    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty("role", "user");
    message->setProperty("content", prompt);
    messages.add(juce::var(message.get()));
    
    requestObj->setProperty("messages", messages);
    
    juce::String requestBody = juce::JSON::toString(juce::var(requestObj.get()));
    
    // Prepare request with POST data
    juce::URL url = juce::URL(apiEndpoint)
        .withPOSTData(requestBody);
    
    juce::String extraHeaders = "Content-Type: application/json\r\n"
                                "Authorization: Bearer " + apiKey + "\r\n";
    
    std::unique_ptr<juce::InputStream> stream(url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders(extraHeaders)
            .withConnectionTimeoutMs(10000)
            .withNumRedirectsToFollow(0)));
    
    if (stream == nullptr)
    {
        DBG("Failed to create input stream for API request");
        return std::nullopt;
    }
    
    juce::String response = stream->readEntireStreamAsString();
    
    // Parse response
    juce::var parsedResponse;
    juce::Result result = juce::JSON::parse(response, parsedResponse);
    
    if (!result.wasOk())
    {
        DBG("Failed to parse API response: " + result.getErrorMessage());
        return std::nullopt;
    }
    
    // Extract content from response
    if (auto* obj = parsedResponse.getDynamicObject())
    {
        if (auto choices = obj->getProperty("choices").getArray())
        {
            if (choices->size() > 0)
            {
                if (auto* choice = choices->getUnchecked(0).getDynamicObject())
                {
                    if (auto* msg = choice->getProperty("message").getDynamicObject())
                    {
                        return msg->getProperty("content").toString();
                    }
                }
            }
        }
    }
    
    DBG("Failed to extract content from API response");
    return std::nullopt;
}

std::vector<Note> LLMGenerator::parseNotesFromJSON(const juce::String& jsonResponse,
                                                   const MidiGenerationParams& params)
{
    std::vector<Note> notes;
    
    // Extract JSON array from response (may have markdown code blocks)
    juce::String cleanedJSON = jsonResponse.trim();
    if (cleanedJSON.startsWith("```"))
    {
        int firstNewline = cleanedJSON.indexOf("\n");
        int lastBacktick = cleanedJSON.lastIndexOf("```");
        if (firstNewline > 0 && lastBacktick > firstNewline)
        {
            cleanedJSON = cleanedJSON.substring(firstNewline + 1, lastBacktick).trim();
        }
    }
    
    juce::var parsedJSON;
    juce::Result result = juce::JSON::parse(cleanedJSON, parsedJSON);
    
    if (!result.wasOk())
    {
        DBG("Failed to parse notes JSON: " + result.getErrorMessage());
        return notes;
    }
    
    if (!parsedJSON.isArray())
    {
        DBG("Parsed JSON is not an array");
        return notes;
    }
    
    auto* notesArray = parsedJSON.getArray();
    auto scaleNotes = getScaleNotes(params.rootNote, params.scale);
    double quantStep = quantizationToQuarterNotes(params.quantization);
    
    for (int i = 0; i < notesArray->size(); ++i)
    {
        if (auto* noteObj = (*notesArray)[i].getDynamicObject())
        {
            Note note;
            note.pitch = noteObj->getProperty("pitch");
            note.startTime = noteObj->getProperty("start");
            note.duration = noteObj->getProperty("duration");
            note.velocity = noteObj->getProperty("velocity");
            
            // Validate and quantize
            note.pitch = juce::jlimit(0, 127, note.pitch);
            note.velocity = juce::jlimit(60, 100, note.velocity);
            
            // Quantize start and duration
            note.startTime = std::round(note.startTime / quantStep) * quantStep;
            note.duration = std::max(quantStep, std::round(note.duration / quantStep) * quantStep);
            
            // Ensure note is in scale
            if (!scaleNotes.empty())
            {
                auto it = std::min_element(scaleNotes.begin(), scaleNotes.end(),
                    [&](int a, int b) {
                        return std::abs(a - note.pitch) < std::abs(b - note.pitch);
                    });
                note.pitch = *it;
            }
            
            notes.push_back(note);
        }
    }
    
    return notes;
}
