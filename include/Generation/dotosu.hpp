#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

namespace eppyphany::Generation {
    struct MapConfig {
        // Metadata
        std::string Name;
        std::string Author; // producer(s)
        std::string Creator; // map creator
        std::string Source;
        std::filesystem::path AudioFile;
        std::string DifficultyName;
        unsigned int BPM;
        unsigned int Keys;
        double PreviewTime;

        // Difficulty
        double HPDrainRate = 8.0;
        double OverallDifficulty = 8.0; // accuracy
        double StarRating = 5.0;
    };

    struct TimingPoint {
        double OffsetMs;
        double BeatLength;
        int Meter;
        int SampleSet;
        int SampleIndex;
        int Volume;
        int Uninherited; // 1 = uninherited (BPM change), 0 = inherited (SV change)
        int Effects; // bit mask (0/1 = kiai toggle, 3/8 = omit first bar line)
    };

    enum HitObjectType {
        Note = 1,
        LongNote = 128,
    };

    struct HitObject {
        int Column = 1;
        HitObjectType Type = HitObjectType::Note;
        double HitTime = 0;
        double ReleaseTime = -1;
    };

    class dotosu {
        public:
            const double MillisecondsPerBeat;
            const MapConfig Config;

            dotosu(const MapConfig& _config);

            void AddHitObject(HitObject _hitObject);
            void AddHitObject(int columnIndex, double hitTime, double releaseTime = -1);
            void ClearHitObjects();
            void AddTimingPoint(TimingPoint tp);
            void ClearTimingPoints();
            const std::vector<HitObject>& GetHitObjects() const;
            bool Save(const std::filesystem::path& out);

        private:
            std::ifstream inputStream_;
            std::vector<HitObject> hitObjects_;
            std::vector<TimingPoint> timingPoints_;

            int _calculateLaneX(int columnIndex) const;
    };
}