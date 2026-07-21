#pragma once

#include "DSP/AudioLoader.hpp"
#include "dotosu.hpp"
#include "DSP/Analyzer.hpp"
#include "DSP/Separator.hpp"
#include <vector>
#include <cstdint>

using namespace eppyphany::DSP;

namespace eppyphany::Generation {
    enum Patterns : uint64_t {
        Staircase       = 1ULL << 0,
        Roll            = 1ULL << 1,
        SplitRoll       = 1ULL << 2,
        TwoHandTrill    = 1ULL << 3,
        OneHandTrill    = 1ULL << 4,
        JumpTrill       = 1ULL << 5,
        SplitTrill      = 1ULL << 6,
        RunningMan      = 1ULL << 7,
        Gallop          = 1ULL << 8,
        Ladder          = 1ULL << 9,
        Flam            = 1ULL << 10,
        Anchor          = 1ULL << 11,
        SingleStream    = 1ULL << 12,
        LightHandstream = 1ULL << 13,
        DenseHandstream = 1ULL << 14,
        LightJumpstream = 1ULL << 15,
        DenseJumpstream = 1ULL << 16,
        QuadStream      = 1ULL << 17,
        BrokenStream    = 1ULL << 18,
        MiniJack        = 1ULL << 19,
        ChordJack       = 1ULL << 20,
        JumpJack        = 1ULL << 21,
        LongJack        = 1ULL << 22,
        Shield          = 1ULL << 23,
        ReverseShield   = 1ULL << 24,
        Inverse         = 1ULL << 25,

        // 7k+ exclusive
        DoubleStair = 1ULL << 26,
        ChordStream = 1ULL << 27,
        Symmetrical = 1ULL << 28,
        ChordTrill  = 1ULL << 29,
        Delay       = 1ULL << 30
    };

    struct OnsetCandidate {
        double TimestampMs;
        double Strength;
        int StemIndex;
    };

    struct ReviewFlag {
        double StartMs;
        double EndMs;
        double TimingEntropyScore; // max across columns in this region
        double ColumnEntropyScore; // per-hand sum
    };

    struct ColumnState {
        std::vector<double> LastHit;
        std::vector<double> AverageGap;
        std::vector<double> TimingEntropy;
        std::vector<double> ColumnEntropy;
        std::vector<int> HitCounts;

        ColumnState(int totalColumns)
            : LastHit(totalColumns, -1), AverageGap(totalColumns, 0.0),
            TimingEntropy(totalColumns, 0.0), HitCounts(totalColumns, 0),
            ColumnEntropy(2, 0.0) {}
    };

    struct BeamPath {
        BeamPath(ColumnState _state, std::vector<int> _chosenColumns, double cost_ = 0.0) 
            : State(_state), ChosenColumns(_chosenColumns), Cost(cost_) {};

        ColumnState State;
        std::vector<int> ChosenColumns; // one per candidate processed in this window
        double Cost = 0.0;
    };

    class Composer {
        public:
            Composer(int fftSize = 1024, int hopSize = 512);

            std::vector<HitObject> Compose(const MapConfig& config, Audio decoded, uint64_t availablePatterns = ~0ULL & ~Patterns::Inverse);

        private:
            int fftSize_;
            int hopSize_;

            DSP::Analyzer analyzer_;
            DSP::Separator separator_;

            std::vector<double> columnEntropy_; // per-hand
            std::vector<double> timingEntropy_; // per-column
            std::vector<double> lastHitInColumn_; // per-column, for gap/recency tracking
            std::vector<double> avgGapInColumn_; // per-column running average gap, EMA basis for timingEntropy_
            std::vector<int> columnHitCounts_; // per-column, basis for per-hand Shannon entropy

            std::vector<OnsetCandidate> _pickOnsets(const std::vector<DSP::AudioFrame>& timeline, int stemIndex);
            std::vector<OnsetCandidate> _buildCandidatePool(const DSP::Audio& audio);

            double _getOptimalTimingGap(double sr);
            void _updateEntropy(int column, double timestampMs, int totalColumns);

            std::vector<HitObject> _beamSearch(const std::vector<OnsetCandidate>& candidates,
                const MapConfig& config, uint64_t availablePatterns);

            std::vector<ReviewFlag> _review(const std::vector<HitObject>& hitObjects, int totalColumns);
    };
}