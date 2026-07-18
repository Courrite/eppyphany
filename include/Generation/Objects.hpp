#pragma once

#include <string>
#include <filesystem>

namespace eppyphany::Generation {
    struct dotosuFileConfig {
        // Metadata
        std::string Name;
        std::string Author; // producer(s)
        std::string Creator; // map creator
        std::string Source;
        std::filesystem::path AudioFile;
        std::string DifficultyName;
        unsigned int BPM;
        unsigned int Keys;
        int PreviewTime;

        // Difficulty
        float HPDrainRate = 8.0f;
        float OverallDifficulty = 8.0f; // accuracy
        float StarRating = 5.0f;
    };

    enum HitObjectType {
        Note = 1,
        LongNote = 128,
    };

    struct HitObject {
        int X = 64;
        int Column = 1;
        HitObjectType Type = HitObjectType::Note;
        int HitTime = 0;
        int ReleaseTime = -1;
    };

    struct TimingPoint {
        int OffsetMs;
        double BeatLength;
        int Meter;
        int SampleSet;
        int SampleIndex;
        int Volume;
        int Uninherited; // 1 = uninherited (BPM change), 0 = inherited (SV change)
        int Effects;
    };
    
    struct GeneratorConfig {
        unsigned int Keys = 4;
        float TargetStarRating = 5.0f;
        unsigned int RandomSeed = 0;
    };

    struct PlacedNote {
        int Column;
        int HitTimeMs;
        int ReleaseTimeMs = -1;
    };

    struct KiaiSection {
        int StartMs;
        int EndMs;
    };
}