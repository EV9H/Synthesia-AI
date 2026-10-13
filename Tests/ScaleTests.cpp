#include <gtest/gtest.h>
#include "Generators/MidiGenerator.h"

class TestableGenerator : public MidiGenerator
{
public:
    juce::MidiBuffer generate(const MidiGenerationParams& params) override
    {
        return juce::MidiBuffer();
    }
    
    // Expose protected methods for testing
    using MidiGenerator::getScaleNotes;
    using MidiGenerator::quantizationToQuarterNotes;
};

TEST(ScaleTest, MajorScaleHasSevenNotes)
{
    TestableGenerator gen;
    auto scale = gen.getScaleNotes(0, ScaleType::Major); // C Major
    
    EXPECT_FALSE(scale.empty());
    
    // Count notes in one octave (first 7 notes)
    std::set<int> uniqueInOctave;
    for (int note : scale)
    {
        uniqueInOctave.insert(note % 12);
        if (uniqueInOctave.size() >= 7)
            break;
    }
    
    EXPECT_EQ(uniqueInOctave.size(), 7);
}

TEST(ScaleTest, MinorScaleCorrectIntervals)
{
    TestableGenerator gen;
    auto scale = gen.getScaleNotes(0, ScaleType::Minor); // C Minor
    
    EXPECT_FALSE(scale.empty());
    
    // C Minor scale intervals: C(0), D(2), Eb(3), F(5), G(7), Ab(8), Bb(10)
    // Check first note of each octave in range
    bool foundRoot = false;
    bool foundMinorThird = false;
    
    for (int note : scale)
    {
        int pitch = note % 12;
        if (pitch == 0) foundRoot = true;
        if (pitch == 3) foundMinorThird = true;
    }
    
    EXPECT_TRUE(foundRoot);
    EXPECT_TRUE(foundMinorThird);
}

TEST(ScaleTest, PentatonicScaleHasFiveNotes)
{
    TestableGenerator gen;
    auto scale = gen.getScaleNotes(0, ScaleType::Pentatonic);
    
    EXPECT_FALSE(scale.empty());
    
    // Count unique notes in one octave
    std::set<int> uniqueInOctave;
    for (int note : scale)
    {
        uniqueInOctave.insert(note % 12);
        if (uniqueInOctave.size() >= 5)
            break;
    }
    
    EXPECT_EQ(uniqueInOctave.size(), 5);
}

TEST(ScaleTest, AllNotesInValidMIDIRange)
{
    TestableGenerator gen;
    
    for (int root = 0; root < 12; ++root)
    {
        auto scale = gen.getScaleNotes(root, ScaleType::Major);
        for (int note : scale)
        {
            EXPECT_GE(note, 0);
            EXPECT_LT(note, 128);
        }
    }
}

TEST(ScaleTest, QuantizationConversion)
{
    TestableGenerator gen;
    
    EXPECT_DOUBLE_EQ(gen.quantizationToQuarterNotes(Quantization::Sixteenth), 0.25);
    EXPECT_DOUBLE_EQ(gen.quantizationToQuarterNotes(Quantization::Eighth), 0.5);
    EXPECT_DOUBLE_EQ(gen.quantizationToQuarterNotes(Quantization::Quarter), 1.0);
    EXPECT_DOUBLE_EQ(gen.quantizationToQuarterNotes(Quantization::Half), 2.0);
}
