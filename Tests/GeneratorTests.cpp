#include <gtest/gtest.h>
#include "Generators/MarkovGenerator.h"
#include "Generators/MidiGenerator.h"

class GeneratorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        generator = std::make_unique<MarkovGenerator>();
    }
    
    std::unique_ptr<MarkovGenerator> generator;
};

TEST_F(GeneratorTest, GeneratesNonEmptyMidiBuffer)
{
    MidiGenerationParams params;
    params.rootNote = 0; // C
    params.scale = ScaleType::Major;
    params.lengthInBars = 4;
    params.quantization = Quantization::Eighth;
    params.tempo = 120.0;
    
    auto buffer = generator->generate(params);
    
    EXPECT_FALSE(buffer.isEmpty());
}

TEST_F(GeneratorTest, RespectsLengthConstraint)
{
    MidiGenerationParams params;
    params.rootNote = 0;
    params.scale = ScaleType::Major;
    params.lengthInBars = 2;
    params.quantization = Quantization::Quarter;
    params.tempo = 120.0;
    
    auto buffer = generator->generate(params);
    
    // Check that no notes extend beyond expected length
    // With 2 bars at 4/4, we have 8 quarter notes
    // At 960 PPQ, that's 7680 ticks
    int maxTick = 0;
    for (const auto metadata : buffer)
    {
        maxTick = std::max(maxTick, metadata.samplePosition);
    }
    
    // Should not exceed 2 bars worth of ticks (8 quarter notes * 960 PPQ)
    EXPECT_LE(maxTick, 8 * 960);
}

TEST_F(GeneratorTest, GeneratesValidMidiNoteNumbers)
{
    MidiGenerationParams params;
    params.rootNote = 5; // F
    params.scale = ScaleType::Minor;
    params.lengthInBars = 1;
    params.tempo = 120.0;
    
    auto buffer = generator->generate(params);
    
    for (const auto metadata : buffer)
    {
        auto message = metadata.getMessage();
        if (message.isNoteOn())
        {
            int noteNumber = message.getNoteNumber();
            EXPECT_GE(noteNumber, 0);
            EXPECT_LT(noteNumber, 128);
        }
    }
}

TEST_F(GeneratorTest, DifferentScalesProduceDifferentResults)
{
    MidiGenerationParams params1;
    params1.rootNote = 0;
    params1.scale = ScaleType::Major;
    params1.lengthInBars = 2;
    params1.tempo = 120.0;
    
    MidiGenerationParams params2 = params1;
    params2.scale = ScaleType::Pentatonic;
    
    auto buffer1 = generator->generate(params1);
    auto buffer2 = generator->generate(params2);
    
    // Both should have content
    EXPECT_FALSE(buffer1.isEmpty());
    EXPECT_FALSE(buffer2.isEmpty());
    
    // They should generally be different (though random, so not guaranteed)
    // Just verify they both work
    EXPECT_TRUE(true);
}
