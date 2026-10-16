#include "GenreProfiles.h"

namespace
{
    const std::vector<int> arpUp{ 0, 1, 2, 3, 4, 5, 6, 7 };
    const std::vector<int> arpUpDown{ 0, 1, 2, 3, 4, 3, 2, 1 };
    const std::vector<int> arpSkip{ 0, 2, 1, 3, 2, 4, 3, 5 };
    const std::vector<int> arpRolling{ 0, 1, 2, 1, 3, 2, 4, 2 };

    GenreProfile makePop()
    {
        GenreProfile p{};
        p.genre = Genre::Pop;
        p.defaultTempo = 110.0;
        p.defaultGrid = Quantization::Eighth;
        p.defaultExtension = 3;
        p.majorProgressions = { { "1 5 67 4a9" }, { "67 4 1 5s4" }, { "1 67 4 5" }, { "4 1 5 67" },
                                { "1 5s4 6 4a9" }, { "1a9 5 67 4s4" }, { "1 5D 67 4" } };
        p.minorProgressions = { { "1 67 3 7" }, { "67 7 1 5D" }, { "1 7 67 5D" }, { "1 67 4 5D" },
                                { "1 47 67 5D" } };

        p.chordStyle = ChordStyle::Pulse;
        p.chordPatterns = { "X-------x-------", "X-----x-------x-", "X-------x---x---", "X---x---X-------" };
        p.chordVoicing = { 55, 74, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R-------R-------", "R-------R---F---", "R---....R-------", "R-------F---A---" };
        p.bassLow = 36;
        p.bassHigh = 47;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 60, 71, 0.52, 0.48, 0.34, 0.34, 2, false, false, 92 };
        p.leadMelody.intervalWeights = { 0.05, 0.1, 0.25, 0.7, 1.6, 2.8, 1.1, 2.8, 1.6, 0.7, 0.25, 0.1, 0.05 };

        p.pianoStyle = PianoStyle::Broken;
        p.pianoVoicing = { 62, 74, 3, false, false };

        p.drums.grooves = {
            { "X.......X.......", "....X.g.....X...", "", "x...x...x...x...", "............x...", "" },
            { "X.x.....X.......", "....X.......X...", "", "x...x...x.x.x...", "................", "" },
            { "X.......X.x.....", "....X.......X...", "", "x...x...x...x...", "........x.......", "" },
            { "X...x...X.......", "....X.g.....X...", "", "x...x...x...x...", "............x...", "" },
        };
        p.drums.hatSwing = 0.50;
        p.drums.humanizeMs = 10.0;
        p.drums.kickVelocity = 114;
        p.drums.snareVelocity = 110;
        p.drums.hatVelocity = 50;

        p.swing = 0.5;
        p.swingSixteenths = false;
        p.anticipation = 0.35;
        return p;
    }

    GenreProfile makeLoFiJazz()
    {
        GenreProfile p{};
        p.genre = Genre::LoFiJazz;
        p.defaultTempo = 84.0;
        p.defaultGrid = Quantization::Eighth;
        p.defaultExtension = 9;
        p.majorProgressions = { { "27 5 19 67 47 37 27 5" }, { "19 67 27 5 19 47 27 5" },
                                { "4 3 67 27" }, { "27 5 19 67" } };
        p.minorProgressions = { { "2h 5D 19 67 47 37 2h 5D" }, { "27 5D 19 67" },
                                { "67 2h 5D 19 47 1 2h 5D" }, { "1 47 7 3 27 5D 19 67" } };

        p.chordStyle = ChordStyle::Comping;
        // Sustained two-hit comps (4-note stacks). A lone full-bar hold stamps
        // variation_8; short stabs tripped bass_root_conflict (q7 lesson).
        p.chordPatterns = { "X-------x-------", "X-----------x---", "X-----x-........", "X-----x-------x-" };
        p.chordVoicing = { 58, 72, 4, false, false };

        p.bassStyle = BassStyle::Walking;
        p.bassPatterns = { "R-------R-------", "R---F---R---A---", "R-------R---F---" };
        p.bassLow = 32;
        p.bassHigh = 44;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 58, 70, 0.50, 0.52, 0.36, 0.34, 2, true, false, 80 };
        p.leadMelody.intervalWeights = { 0.1, 0.2, 0.4, 0.9, 1.6, 2.4, 1.4, 2.4, 1.6, 0.9, 0.4, 0.2, 0.1 };

        p.pianoStyle = PianoStyle::Comp;
        // Answer the pads instead of doubling their rhythm (call_response ~0.25).
        p.pianoPatterns = { "........X-----x-", "........X-------", "....X-----------", "..x-----X-------" };
        p.pianoVoicing = { 58, 72, 4, false, false };

        p.drums.grooves = {
            { "X.......x.......", "....X.g.....X...", "", "x...x...x...x...", "............x...", "" },
            { "X-----------x---", "....X.......X...", "", "x...x.g.x...x...", "................", "" },
            { "X.....x.X.......", "....X.g.....X...", "", "x...x...x...x.g.", "............x...", "" },
            { "X.......X.......", "....X.g.g...X...", "", "x...x...x.......", "............x...", "" },
        };
        p.drums.rideHats = false;
        p.drums.hatSwing = 0.66;
        p.drums.laidBackMs = 14.0;
        p.drums.hatLagMs = 6.0;
        p.drums.humanizeMs = 11.0;
        p.drums.kickVelocity = 100;
        p.drums.snareVelocity = 96;
        p.drums.hatVelocity = 48;

        p.swing = 0.62;
        p.swingSixteenths = false;
        p.anticipation = 0.25;
        return p;
    }

    GenreProfile makeHouse()
    {
        GenreProfile p{};
        p.genre = Genre::House;
        p.defaultTempo = 124.0;
        p.defaultGrid = Quantization::Sixteenth;
        p.defaultExtension = 7;
        p.majorProgressions = { { "1 5 6 4s4" }, { "67 4 1 5D" }, { "1 5s4 67 4" }, { "1 5 67 4" },
                                { "67 4s4 1 5D" } };
        p.minorProgressions = { { "1 67 3 7" }, { "1 5D 67 4" }, { "1 4 1 4s4" }, { "67 4 1 5D" } };

        p.chordStyle = ChordStyle::Stabs;
        p.chordPatterns = { "..x-..x-..x-..x-", "..x..x....x..x..", "..x-..x-..x..x..", "x-..x-..x-..x-.." };
        p.chordVoicing = { 55, 79, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "..R-..R-......R-", "R-......R-..R-..", "..R-..R-..R-....", "R-......R---A---" };
        p.bassLow = 33;
        p.bassHigh = 45;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 60, 71, 0.56, 0.46, 0.18, 0.20, 2, false, false, 86 };
        p.leadMelody.intervalWeights = { 0.0, 0.05, 0.1, 0.3, 0.8, 1.6, 2.6, 1.6, 0.8, 0.3, 0.1, 0.05, 0.0 };

        p.pianoStyle = PianoStyle::Comp;
        p.pianoPatterns = { "X--x--x---------", "X--x--x---x-----", "........x--x--x-", "x--x------------" };
        p.pianoVoicing = { 60, 76, 3, false, false };

        p.drums.grooves = {
            // Closed hats live on e/a 16ths, not a straight 8th grid (that
            // crushed house drums). Off-beats lift syncopation (s23 was 8.0).
            { "X...X...X...X...", "", "....X.......X...", ".x.x..x..x.x..x.", "..x...x...x...x.", "" },
            { "X...X...X...X.x.", "", "....X.......X...", "x..x.x..x..x.x..", "..x...x...x...x.", "" },
            { "X...X...X...X...", "", "....X.......X...", ".x..x.x..x.x..x.", "......x.......x.", "" },
            { "X...X...X...X...", "", "....X.....x.X...", "..x.x..x.x..x.x.", "..x...x...x.....", "" },
        };
        p.drums.hatSwing = 0.50;
        p.drums.humanizeMs = 9.0;
        p.drums.kickVelocity = 118;
        p.drums.snareVelocity = 112;
        p.drums.hatVelocity = 48;

        p.swing = 0.5;
        p.swingSixteenths = false;
        p.anticipation = 0.0;
        return p;
    }

    GenreProfile makeTrap()
    {
        GenreProfile p{};
        p.genre = Genre::Trap;
        p.defaultTempo = 140.0;
        p.defaultGrid = Quantization::Sixteenth;
        p.defaultExtension = 7;
        p.majorProgressions = { { "67 4 1 5 67 2h 1 5" }, { "67 2h 5D 1 47 67 1 5" }, { "1 67 4 5D" } };
        p.minorProgressions = { { "2h 5D 1 67 47 3 2h 5D" }, { "1 47 67 5D 2h 1 67 4" },
                                { "1 2h 67 5D 1 47 2h 5D" }, { "67 2h 5D 19 1 47 67 5D" } };

        p.chordStyle = ChordStyle::Comping;
        p.chordPatterns = { "X---x---X---x---", "X---x---X---x-x-", "x---X---x---X---", "X-x-X---x---X---" };
        p.chordVoicing = { 55, 74, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R-------..R---R-", "R-----R-----O---", "R---------R-.R--", "R-------R---..A-" };
        p.bassLow = 36;
        p.bassHigh = 45;
        p.slideChance = 0.45;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 60, 72, 0.68, 0.42, 0.14, 0.18, 1, false, false, 88 };
        p.leadMelody.intervalWeights = { 0.15, 0.2, 0.35, 0.6, 1.1, 1.8, 1.8, 1.8, 1.1, 0.6, 0.35, 0.2, 0.15 };

        p.pianoStyle = PianoStyle::Broken;
        p.pianoVoicing = { 62, 74, 3, false, false };

        p.drums.grooves = {
            { "X---------------", "........X.......", "........x.......", "x.x.x.x.x.x.x.x.", "..............x.", "" },
            { "X-----x---------", "........X.......", "........X.......", "x.x.x.x.x.x.xxx.", "............x...", "" },
            { "X-----------x---", "........X.......", "........x.......", "x.x.x.x.x.x.x.x.", "..............x.", "" },
            { "X...x-----------", "........X.......", "........x.......", "x.x.xx.x.x.x.x.x", "..............x.", "" },
        };
        p.drums.hatSwing = 0.50;
        p.drums.humanizeMs = 12.0;
        p.drums.lockKickToBass = true;
        p.drums.kickVelocity = 116;
        p.drums.snareVelocity = 114;
        p.drums.hatVelocity = 50;

        p.swing = 0.5;
        p.swingSixteenths = false;
        p.anticipation = 0.0;
        return p;
    }

    GenreProfile makeRnB()
    {
        GenreProfile p{};
        p.genre = Genre::RnB;
        p.defaultTempo = 88.0;
        p.defaultGrid = Quantization::Sixteenth;
        p.defaultExtension = 9;
        p.majorProgressions = { { "4 3 27 1" }, { "211 5 19 4" }, { "4 3 67 27" } };
        p.minorProgressions = { { "111 4 67 4" }, { "1 47 7 3" }, { "67 27 5D 19" } };

        p.chordStyle = ChordStyle::Comping;
        p.chordPatterns = { "X-----------..x-", "X---------..x---", "X-------..x-x---", "..x-----..x-----" };
        p.chordVoicing = { 58, 74, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R-------R---A---", "R-------A---F---", "R---F---R---A---", "R-----R-----A---" };
        p.bassLow = 36;
        p.bassHigh = 47;
        p.slideChance = 0.2;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 62, 72, 0.38, 0.52, 0.42, 0.30, 2, true, false, 82 };
        p.leadMelody.intervalWeights = { 0.1, 0.2, 0.35, 0.8, 1.5, 2.2, 1.3, 2.2, 1.5, 0.8, 0.35, 0.2, 0.1 };

        p.pianoStyle = PianoStyle::Melodic;
        p.pianoMelody = { 60, 72, 0.26, 0.45, 0.48, 0.30, 1, true, true, 74 };

        p.drums.grooves = {
            { "X..x....X.......", "....X.g.....X...", "", "x.x.x...x.x.x...", "............x...", "" },
            { "X..x....X.x.....", "....X.......X...", "", "x...x.x.x...x...", "................", "" },
            { "X.....x.X..x....", "....X.....g.X...", "", "x.x.x...x...x...", "............x...", "" },
            { "X.x.....X..x....", "....X.g.....X...", "", "x.g.x...x.g.x...", "............x...", "" },
        };
        p.drums.hatSwing = 0.60;
        p.drums.laidBackMs = 14.0;
        p.drums.hatLagMs = 6.0;
        p.drums.humanizeMs = 8.0;
        p.drums.kickVelocity = 106;
        p.drums.snareVelocity = 102;
        p.drums.hatVelocity = 46;

        p.swing = 0.56;
        p.swingSixteenths = true;
        p.anticipation = 0.5;
        return p;
    }

    GenreProfile makeCinematic()
    {
        GenreProfile p{};
        p.genre = Genre::Cinematic;
        p.defaultTempo = 80.0;
        p.defaultGrid = Quantization::Eighth;
        p.defaultExtension = 7;
        p.majorProgressions = { { "1s4 5 6a9 4 1 5s4 67 4a9" }, { "1 5s4 67 4a9" },
                                { "1 b6 b7 1s4 4 5 6a9 1" }, { "1 5 6 3" } };
        p.minorProgressions = { { "1s4 67 3 7 4a9 5D 1 7" }, { "1 67 4a9 5D" },
                                { "1 3s4 7 4 1s4 67 3 7" }, { "1 b6 b7 4a9 1s4 67 3 1" } };

        p.chordStyle = ChordStyle::Sustained;
        p.chordPatterns = { "X---------------", "X-------x-------", "X-----------x---" };
        p.chordVoicing = { 60, 76, 4, false, true };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R---------------", "R-------F-------", "R-------R-------", "R-----------F---" };
        p.bassLow = 32;
        p.bassHigh = 44;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 60, 70, 0.48, 0.48, 0.38, 0.42, 2, false, false, 80 };
        p.leadMelody.intervalWeights = { 0.2, 0.3, 0.5, 0.9, 1.5, 2.0, 1.2, 2.0, 1.5, 0.9, 0.5, 0.3, 0.2 };

        p.pianoStyle = PianoStyle::Arpeggio;
        p.pianoArp = { 62, 76, { arpRolling, arpSkip, arpUpDown }, 0.88, 74 };

        p.drums.grooves = {
            // One ghost after each backbeat — not the q15 wall of g's that
            // crushed loop snare density. Refs still want more snare than 0.47.
            { "X..x....X..x....", "....X.g.....X...", "", ".x...x.x...x....", "", "........x......." },
            { "X.x.....X..x....", "....X.g.....X...", "", ".x.x.....x.x....", "", "......x........." },
            { "X...x...X.x.....", "....X.g.....X...", "", ".x...x.x...x....", "", "............x..." },
            { "X..x..x.X.......", "....X.g.....X...", "", ".x...x.x.x...x..", "", "........x......." },
        };
        p.drums.rideHats = false;
        p.drums.humanizeMs = 14.0;
        p.drums.hatLagMs = 0.0;
        p.drums.kickVelocity = 118;
        p.drums.snareVelocity = 102;
        p.drums.hatVelocity = 44;

        p.swing = 0.5;
        p.swingSixteenths = false;
        p.anticipation = 0.0;
        return p;
    }
}

const GenreProfile& getGenreProfile(Genre genre)
{
    static const std::vector<GenreProfile> profiles{
        makePop(), makeLoFiJazz(), makeHouse(), makeTrap(), makeRnB(), makeCinematic()
    };

    const auto index = static_cast<size_t>(genre);
    return profiles[index < profiles.size() ? index : 0];
}
