#pragma once

#include "DSProcessor.hpp"
#include <vector>
#include <random>

namespace eppyphany::Generation {

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

    class PatternLibrary {
    public:
        const GeneratorConfig Config;
        
        PatternLibrary(const GeneratorConfig& config);

        std::vector<PlacedNote> Generate(const std::vector<AudioFrame>& timeline);

        float LastThresholdMultiplier() const { return lastThresholdMultiplier_; }

    private:
        struct Onset {
            double TimestampMs;
            float Strength;
            float SubBassRatio;
            float MidRatio;
            float HighRatio;
        };

        std::mt19937 rng_;
        float lastThresholdMultiplier_ = 1.0f;

        std::vector<Onset> _detectOnsets(const std::vector<AudioFrame>& timeline, float thresholdMultiplier) const;
        std::vector<PlacedNote> _assignPatterns(const std::vector<Onset>& onsets, const std::vector<AudioFrame>& timeline);
        float _estimateStarRatingFromOnsets(const std::vector<Onset>& onsets, double songDurationMs) const;
        int _pickLane(int previousLane, float subBassRatio, float highRatio);
    };

}