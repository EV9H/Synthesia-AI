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
        p.majorProgressions = { { "1 5 6 4a9" }, { "6 4 1 5s4" }, { "1 6 4 5" }, { "4 1 5 6" } };
        p.minorProgressions = { { "1 6 3 7" }, { "6 7 1 1" }, { "1 7 6 7" } };

        p.chordStyle = ChordStyle::Pulse;
        p.chordPatterns = { "X-------x-------", "X-----x-------x-", "X-------x---x---" };
        p.chordVoicing = { 52, 76, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R-R-R-R-R-R-F-A-", "R---..R-R---..A-", "R-.RR-.RR-.RF-A-" };
        p.bassLow = 33;
        p.bassHigh = 52;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 64, 84, 0.55, 0.3, 0.25, 0.2, 2, false, false, 92 };

        p.pianoStyle = PianoStyle::Broken;
        p.pianoVoicing = { 55, 74, 3, false, false };

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
        p.majorProgressions = { { "2 5 1 1" }, { "1 6 2 5" }, { "4 3 6 2" }, { "2 5 1 6" } };
        p.minorProgressions = { { "1 4 7 3" }, { "2 5D 1 1" }, { "1 6 2 5D" } };

        p.chordStyle = ChordStyle::Comping;
        p.chordPatterns = { "X-----x-........", "X--.....x---....", "..x-----..x-....",
                            "X-----.x-----...", "X---------..x---" };
        p.chordVoicing = { 50, 74, 4, true, false };

        p.bassStyle = BassStyle::Walking;
        p.bassLow = 28;
        p.bassHigh = 50;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 65, 84, 0.35, 0.45, 0.4, 0.25, 2, true, false, 84 };

        p.pianoStyle = PianoStyle::Melodic;
        p.pianoMelody = { 60, 82, 0.3, 0.4, 0.45, 0.2, 1, true, true, 78 };

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
        p.majorProgressions = { { "6 4 1 5" }, { "1 5 6 4" }, { "4 5 6 6" } };
        p.minorProgressions = { { "1 6 3 7" }, { "1 7 6 7" }, { "1 4 1 4" }, { "1 6 7 1" } };

        p.chordStyle = ChordStyle::Stabs;
        p.chordPatterns = { "..x-..x-..x-..x-", "..x..x....x..x..", "..x-..x-..x..x.." };
        p.chordVoicing = { 55, 79, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "..R-..R-..R-..R-", "..R-..O-..R-..R-", "..R-..R-..R-.RA-" };
        p.bassLow = 33;
        p.bassHigh = 50;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Arp;
        p.leadArp = { 62, 88, { arpUpDown, arpUp, arpSkip }, 0.55, 86 };

        p.pianoStyle = PianoStyle::Comp;
        p.pianoPatterns = { "X--x--x---------", "X--x--x---x-----", "x--x--x---x--x--" };
        p.pianoVoicing = { 58, 84, 4, false, false };

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
        p.defaultExtension = 3;
        p.majorProgressions = { { "6 4 1 5" }, { "1 5 6 4", 2.0 }, { "6 3 4 4" } };
        p.minorProgressions = { { "1 6", 2.0 }, { "1 6 3 7" }, { "1 4 6 5D" }, { "1 b2 1 6" } };

        p.chordStyle = ChordStyle::Sustained;
        p.chordVoicing = { 55, 79, 4, false, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R-------..R---R-", "R-----R-----O---", "R---------R-.R--", "R-------R---..A-" };
        p.bassLow = 26;
        p.bassHigh = 43;
        p.slideChance = 0.45;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 72, 91, 0.42, 0.55, 0.35, 0.15, 1, false, false, 86 };

        p.pianoStyle = PianoStyle::Melodic;
        p.pianoMelody = { 60, 79, 0.25, 0.3, 0.5, 0.35, 2, false, true, 76 };

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
        p.majorProgressions = { { "4 3 2 1" }, { "211 5 1 4" }, { "4 3 6 2" } };
        p.minorProgressions = { { "111 4 111 4" }, { "1 4 7 3" }, { "6 7 1 1" } };

        p.chordStyle = ChordStyle::Comping;
        p.chordPatterns = { "X-----------..x-", "X---------..x---", "X-------..x-x---" };
        p.chordVoicing = { 52, 76, 5, true, false };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R--.....R-.F..A-", "R---..TR-...O.A.", "R-----..F-.R..A-" };
        p.bassLow = 28;
        p.bassHigh = 50;
        p.slideChance = 0.15;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 62, 82, 0.45, 0.5, 0.35, 0.25, 2, true, false, 84 };

        p.pianoStyle = PianoStyle::Melodic;
        p.pianoMelody = { 60, 82, 0.3, 0.5, 0.45, 0.2, 1, true, true, 76 };

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
        p.defaultExtension = 3;
        p.majorProgressions = { { "1 5 6 3 4 1 4 5" }, { "1 b6 b7 1" }, { "6 4 1 5s4" } };
        p.minorProgressions = { { "1 6 3 7" }, { "1 6 4 5D" }, { "1 3 7 4" } };

        p.chordStyle = ChordStyle::Sustained;
        p.chordVoicing = { 43, 79, 4, false, true };

        p.bassStyle = BassStyle::Pattern;
        p.bassPatterns = { "R---------------", "R-------F-------" };
        p.bassLow = 28;
        p.bassHigh = 48;
        p.slideChance = 0.0;

        p.leadStyle = LeadStyle::Melody;
        p.leadMelody = { 67, 88, 0.3, 0.15, 0.3, 0.45, 2, false, false, 82 };

        p.pianoStyle = PianoStyle::Arpeggio;
        p.pianoArp = { 48, 84, { arpRolling, arpSkip, arpUpDown }, 0.95, 72 };

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
