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
    
    std::vector<int> channels;
    auto notes = parseNotesFromJSON(*response, params, &channels);

    juce::MidiBuffer buffer;
    for (const auto part : allParts)
    {
        const int channel = partChannel(part);
        std::vector<Note> partNotes;
        for (size_t i = 0; i < notes.size(); ++i)
            if (channels[i] == channel)
                partNotes.push_back(notes[i]);

        if (!partNotes.empty())
            buffer.addEvents(notesToMidiBuffer(partNotes, params.tempo, 960.0, channel), 0, -1, 0);
    }
    return buffer;
}

namespace
{
    const char* partGuidance(Part part)
    {
        switch (part)
        {
            case Part::Chords: return "chords: 3-5 note voicings (MIDI 48-79) with smooth voice leading, rhythm suited to the genre";
            case Part::Bass:   return "bass: monophonic, MIDI 28-52, roots/fifths on strong beats with approach notes into chord changes";
            case Part::Synth:  return "synth: lead melody or arpeggio, MIDI 62-91, chord tones on strong beats, a repeated motif with variation";
            case Part::Piano:  return "piano: melodic line or comping, MIDI 48-84, complementing the synth";
            case Part::Drums:  return "drums: General MIDI percussion (kick 36, snare 38, clap 39, closed hat 42, open hat 46, toms 45/47/50, crash 49) on a groove that matches the genre; include ghost notes and a fill at phrase ends";
            case Part::All:    break;
        }
        return "";
    }
}

juce::String LLMGenerator::buildPrompt(const MidiGenerationParams& params)
{
    juce::String scaleNames[] = {"Major", "Minor", "Dorian", "Mixolydian", "Pentatonic"};
    juce::String noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    
    juce::String prompt;
    prompt << "Generate MIDI with the following specifications:\n"
           << "- Key: " << noteNames[params.rootNote] << " " << scaleNames[static_cast<int>(params.scale)] << "\n"
           << "- Genre: " << genreName(params.genre) << "\n"
           << "- Tempo: " << params.tempo << " BPM\n"
           << "- Length: " << params.lengthInBars << " bars (4/4 time)\n"
           << "- Style: " << juce::String(params.stylePrompt) << "\n";

    if (!params.progression.empty())
    {
        prompt << "- Chord progression (write over these chords):";
        for (const auto& chord : params.progression)
            prompt << " " << juce::String(chord.symbol) << " @" << chord.startBeat
                   << "-" << (chord.startBeat + chord.lengthBeats);
        prompt << " (beats)\n";
    }

    prompt << "- Parts to write:\n";
    for (const auto part : allParts)
        if (params.part == Part::All || params.part == part)
            prompt << "  - " << partGuidance(part) << "\n";

    prompt << "\nReturn ONLY a JSON array of note objects with this exact format:\n"
           << "[{\"part\": \"synth\", \"pitch\": 60, \"start\": 0.0, \"duration\": 1.0, \"velocity\": 80}, ...]\n"
           << "where:\n"
           << "- part: one of \"chords\", \"bass\", \"synth\", \"piano\", \"drums\"\n"
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
                                                   const MidiGenerationParams& params,
                                                   std::vector<int>* channels)
{
    const int defaultChannel = params.part == Part::All ? partChannel(Part::Synth) : partChannel(params.part);
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

            const auto partText = noteObj->getProperty("part").toString().toLowerCase();
            const bool isDrum = partText == "drums" || params.part == Part::Drums;
            
            note.pitch = juce::jlimit(0, 127, note.pitch);
            note.velocity = isDrum ? juce::jlimit(1, 127, note.velocity)
                                   : juce::jlimit(60, 100, note.velocity);
            
            note.startTime = std::round(note.startTime / quantStep) * quantStep;
            note.duration = std::max(quantStep, std::round(note.duration / quantStep) * quantStep);
            
            if (!isDrum && !scaleNotes.empty())
            {
                auto it = std::min_element(scaleNotes.begin(), scaleNotes.end(),
                    [&](int a, int b) {
                        return std::abs(a - note.pitch) < std::abs(b - note.pitch);
                    });
                note.pitch = *it;
            }
            
            notes.push_back(note);

            if (channels != nullptr)
            {
                int channel = defaultChannel;
                for (const auto part : allParts)
                    if (partText == juce::String(partName(part)).toLowerCase())
                        channel = partChannel(part);

                if (params.part != Part::All)
                    channel = defaultChannel;
                channels->push_back(channel);
            }
        }
    }
    
    return notes;
}
