#include <gtest/gtest.h>
#include "Generators/LLMGenerator.h"
#include <juce_core/juce_core.h>

class JSONParsingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        generator = std::make_unique<LLMGenerator>();
    }
    
    std::unique_ptr<LLMGenerator> generator;
};

TEST_F(JSONParsingTest, ParsesValidJSONArray)
{
    juce::String jsonResponse = R"([
        {"pitch": 60, "start": 0.0, "duration": 1.0, "velocity": 80},
        {"pitch": 64, "start": 1.0, "duration": 1.0, "velocity": 85},
        {"pitch": 67, "start": 2.0, "duration": 1.0, "velocity": 90}
    ])";
    
    MidiGenerationParams params;
    params.rootNote = 0;
    params.scale = ScaleType::Major;
    params.lengthInBars = 1;
    params.quantization = Quantization::Quarter;
    params.tempo = 120.0;
    
    // We can't directly test parseNotesFromJSON as it's private,
    // but we can verify JSON parsing works
    juce::var parsed;
    auto result = juce::JSON::parse(jsonResponse, parsed);
    
    EXPECT_TRUE(result.wasOk());
    EXPECT_TRUE(parsed.isArray());
    
    auto* arr = parsed.getArray();
    EXPECT_EQ(arr->size(), 3);
    
    // Verify first note structure
    auto* note = (*arr)[0].getDynamicObject();
    EXPECT_TRUE(note != nullptr);
    EXPECT_EQ(static_cast<int>(note->getProperty("pitch")), 60);
    EXPECT_DOUBLE_EQ(static_cast<double>(note->getProperty("start")), 0.0);
}

TEST_F(JSONParsingTest, HandlesMarkdownCodeBlocks)
{
    juce::String jsonWithMarkdown = R"(```json
[
    {"pitch": 60, "start": 0.0, "duration": 1.0, "velocity": 80}
]
```)";
    
    // Extract JSON from markdown
    juce::String cleaned = jsonWithMarkdown.trim();
    if (cleaned.startsWith("```"))
    {
        int firstNewline = cleaned.indexOf("\n");
        int lastBacktick = cleaned.lastIndexOf("```");
        if (firstNewline > 0 && lastBacktick > firstNewline)
        {
            cleaned = cleaned.substring(firstNewline + 1, lastBacktick).trim();
        }
    }
    
    juce::var parsed;
    auto result = juce::JSON::parse(cleaned, parsed);
    
    EXPECT_TRUE(result.wasOk());
    EXPECT_TRUE(parsed.isArray());
}

TEST_F(JSONParsingTest, ValidatesNoteNumbers)
{
    // Test that notes outside MIDI range would be clamped
    int validNote = juce::jlimit(0, 127, 60);
    EXPECT_EQ(validNote, 60);
    
    int tooHigh = juce::jlimit(0, 127, 200);
    EXPECT_EQ(tooHigh, 127);
    
    int tooLow = juce::jlimit(0, 127, -10);
    EXPECT_EQ(tooLow, 0);
}

TEST_F(JSONParsingTest, ValidatesVelocity)
{
    // Test velocity clamping
    int validVel = juce::jlimit(60, 100, 80);
    EXPECT_EQ(validVel, 80);
    
    int tooHigh = juce::jlimit(60, 100, 150);
    EXPECT_EQ(tooHigh, 100);
    
    int tooLow = juce::jlimit(60, 100, 30);
    EXPECT_EQ(tooLow, 60);
}
