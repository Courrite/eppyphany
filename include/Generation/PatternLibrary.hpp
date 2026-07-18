#pragma once

#include "Objects.hpp"
#include "DSProcessor.hpp"
#include <vector>
#include <random>

namespace eppyphany::Generation {
    class PatternLibrary {
    public:
        const GeneratorConfig Config;

        PatternLibrary(const GeneratorConfig& config);

        std::vector<PlacedNote> Generate(const std::vector<AudioFrame>& timeline);

        std::vector<KiaiSection> DetectKiaiSections(const std::vector<AudioFrame>& timeline) const;

        float LastThresholdMultiplier() const { return lastThresholdMultiplier_; }
        double LastEstimatedStarRating() const { return lastEstimatedSr_; }

    private:
        struct Onset {
            double TimestampMs;
            float Strength;
            float SubBassRatio;
            float MidRatio;
            float HighRatio;

            float RhythmicRegularity;
            float BrightnessTrend;
            bool IsSteadyKickPattern;
        };

        std::mt19937 rng_;
        float lastThresholdMultiplier_ = 1.0f;
        double lastEstimatedSr_ = 0.0;

        std::vector<Onset> _detectOnsets(const std::vector<AudioFrame>& timeline, float thresholdMultiplier) const;
        void _computeAuralFeatures(std::vector<Onset>& onsets) const;
        std::vector<PlacedNote> _assignPatterns(const std::vector<Onset>& onsets, const std::vector<AudioFrame>& timeline);
        int _pickLane(int previousLane, float subBassRatio, float highRatio);

        double _rateNotes(const std::vector<PlacedNote>& notes) const;
    };

}