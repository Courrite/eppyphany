#include "Generation/PatternLibrary.hpp"
#include "Generation/dotosu.hpp"
#include "Generation/Objects.hpp"
#include "Difficulty/ManiaDifficultyCalculator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace eppyphany::Generation {

    PatternLibrary::PatternLibrary(const GeneratorConfig& config)
        : Config(config), rng_(config.RandomSeed != 0 ? config.RandomSeed : std::random_device{}()) {}

    std::vector<PatternLibrary::Onset> PatternLibrary::_detectOnsets(
        const std::vector<AudioFrame>& timeline, float thresholdMultiplier) const {

        std::vector<Onset> onsets;
        if (timeline.size() < 3) return onsets;

        const double windowMs = 1000.0;
        const double minGapMs = 45.0;

        float peakFlux = 0.0f;
        for (const auto& f : timeline) peakFlux = std::max(peakFlux, f.TotalFlux);
        if (peakFlux <= 0.0f) return onsets;

        struct Candidate { size_t frameIdx; float zScore; };
        std::vector<Candidate> candidates;

        for (size_t i = 1; i + 1 < timeline.size(); ++i) {
            const auto& cur = timeline[i];

            double windowStart = cur.TimestampMs - windowMs;
            float sum = 0.0f, sumSq = 0.0f;
            int count = 0;
            for (size_t j = i; j-- > 0;) {
                if (timeline[j].TimestampMs < windowStart) break;
                sum += timeline[j].TotalFlux;
                sumSq += timeline[j].TotalFlux * timeline[j].TotalFlux;
                ++count;
            }
            if (count < 4) continue;

            float mean = sum / count;
            float variance = (sumSq / count) - (mean * mean);
            float stddev = variance > 0.0f ? std::sqrt(variance) : 0.0f;
            if (stddev <= 1e-6f) continue;

            float zScore = (cur.TotalFlux - mean) / stddev;

            bool isLocalMax = cur.TotalFlux >= timeline[i - 1].TotalFlux &&
                               cur.TotalFlux >= timeline[i + 1].TotalFlux;
            bool isAboveFloor = zScore > thresholdMultiplier && cur.TotalFlux > 0.0f;

            if (isLocalMax && isAboveFloor) {
                candidates.push_back({ i, zScore });
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.zScore > b.zScore;
        });

        std::vector<double> acceptedTimestamps;
        for (const auto& c : candidates) {
            double ts = timeline[c.frameIdx].TimestampMs;
            auto it = std::lower_bound(acceptedTimestamps.begin(), acceptedTimestamps.end(), ts);
            bool conflictsBefore = it != acceptedTimestamps.begin() && (ts - *std::prev(it)) < minGapMs;
            bool conflictsAfter = it != acceptedTimestamps.end() && (*it - ts) < minGapMs;
            if (!conflictsBefore && !conflictsAfter) {
                acceptedTimestamps.insert(it, ts);
            }
        }

        onsets.reserve(acceptedTimestamps.size());
        size_t candidateFrame = 0;
        for (double ts : acceptedTimestamps) {
            while (candidateFrame < timeline.size() && timeline[candidateFrame].TimestampMs < ts) ++candidateFrame;
            if (candidateFrame >= timeline.size()) break;
            const auto& cur = timeline[candidateFrame];

            float bandTotal = cur.SubBassEnergy + cur.MidEnergy + cur.HighEnergy;
            Onset o{};
            o.TimestampMs = cur.TimestampMs;
            o.Strength = std::min(1.0f, cur.TotalFlux / peakFlux);
            if (bandTotal > 0.0f) {
                o.SubBassRatio = cur.SubBassEnergy / bandTotal;
                o.MidRatio = cur.MidEnergy / bandTotal;
                o.HighRatio = cur.HighEnergy / bandTotal;
            } else {
                o.SubBassRatio = o.MidRatio = o.HighRatio = 1.0f / 3.0f;
            }
            // aural feature fields filled in later by _computeAuralFeatures
            o.RhythmicRegularity = 0.0f;
            o.BrightnessTrend = 0.0f;
            o.IsSteadyKickPattern = false;
            onsets.push_back(o);
        }

        return onsets;
    }

    void PatternLibrary::_computeAuralFeatures(std::vector<Onset>& onsets) const {
        if (onsets.empty()) return;

        const size_t regularityWindow = 5;

        for (size_t i = 0; i < onsets.size(); ++i) {
            if (i < 2) {
                onsets[i].RhythmicRegularity = 0.0f; // not enough history yet
                continue;
            }

            size_t start = (i >= regularityWindow) ? (i - regularityWindow) : 0;
            std::vector<double> gaps;
            for (size_t j = start + 1; j <= i; ++j) {
                gaps.push_back(onsets[j].TimestampMs - onsets[j - 1].TimestampMs);
            }
            if (gaps.size() < 2) {
                onsets[i].RhythmicRegularity = 0.0f;
                continue;
            }

            double meanGap = 0.0;
            for (double g : gaps) meanGap += g;
            meanGap /= gaps.size();

            double variance = 0.0;
            for (double g : gaps) variance += (g - meanGap) * (g - meanGap);
            variance /= gaps.size();
            double stddev = std::sqrt(variance);

            float coefficientOfVariation = (meanGap > 0.0) ? static_cast<float>(stddev / meanGap) : 1.0f;
            onsets[i].RhythmicRegularity = std::clamp(1.0f - coefficientOfVariation, 0.0f, 1.0f);

            float avgSubBassRatio = 0.0f;
            for (size_t j = start; j <= i; ++j) avgSubBassRatio += onsets[j].SubBassRatio;
            avgSubBassRatio /= static_cast<float>(i - start + 1);

            onsets[i].IsSteadyKickPattern = onsets[i].RhythmicRegularity > 0.7f && avgSubBassRatio > 0.45f;
        }

        const size_t brightnessWindow = 4;
        for (size_t i = 0; i < onsets.size(); ++i) {
            if (i < 1) {
                onsets[i].BrightnessTrend = 0.0f;
                continue;
            }
            size_t start = (i >= brightnessWindow) ? (i - brightnessWindow) : 0;
            float first = onsets[start].HighRatio;
            float last = onsets[i].HighRatio;
            onsets[i].BrightnessTrend = std::clamp((last - first) * 3.0f, -1.0f, 1.0f);
        }
    }

    int PatternLibrary::_pickLane(int previousLane, float subBassRatio, float highRatio) {
        unsigned int keys = std::max(1u, Config.Keys);
        std::vector<float> weights(keys, 1.0f);

        for (unsigned int lane = 0; lane < keys; ++lane) {
            float position = keys > 1 ? static_cast<float>(lane) / (keys - 1) : 0.5f;
            weights[lane] += subBassRatio * (1.0f - position) * 2.0f;
            weights[lane] += highRatio * position * 2.0f;
        }

        if (previousLane >= 0 && previousLane < static_cast<int>(keys)) {
            weights[previousLane] *= 0.4f;
        }

        std::discrete_distribution<int> dist(weights.begin(), weights.end());
        return dist(rng_);
    }

    std::vector<PlacedNote> PatternLibrary::_assignPatterns(
        const std::vector<Onset>& onsets, const std::vector<AudioFrame>& timeline) {

        std::vector<PlacedNote> notes;
        if (onsets.empty()) return notes;

        std::uniform_real_distribution<float> chance(0.0f, 1.0f);

        const double jackWindowMs = 180.0;
        const double maxHoldMs = 2000.0;
        const double minHoldMs = 150.0;

        int previousLane = -1;
        double previousTimestampMs = -1e9;

        for (size_t i = 0; i < onsets.size(); ++i) {
            const auto& onset = onsets[i];
            double gapToPrev = onset.TimestampMs - previousTimestampMs;

            bool suppressComplexity = onset.IsSteadyKickPattern;

            bool wantsJack = !suppressComplexity &&
                              previousLane >= 0 &&
                              gapToPrev < jackWindowMs &&
                              onset.Strength > 0.5f &&
                              chance(rng_) < 0.35f;

            float jumpChanceBias = onset.BrightnessTrend > 0.0f ? onset.BrightnessTrend * 0.15f : 0.0f;
            bool wantsJump = !suppressComplexity &&
                              !wantsJack &&
                              onset.Strength > 0.75f &&
                              Config.Keys >= 2 &&
                              chance(rng_) < (0.30f + jumpChanceBias);

            int mainLane = wantsJack ? previousLane : _pickLane(previousLane, onset.SubBassRatio, onset.HighRatio);
            int hitTimeMs = static_cast<int>(onset.TimestampMs);

            int releaseTimeMs = -1;
            {
                size_t frameIdx = 0;
                for (; frameIdx < timeline.size(); ++frameIdx) {
                    if (timeline[frameIdx].TimestampMs >= onset.TimestampMs) break;
                }
                if (frameIdx < timeline.size()) {
                    float baseline = timeline[frameIdx].SubBassEnergy + timeline[frameIdx].MidEnergy + timeline[frameIdx].HighEnergy;
                    double nextOnsetMs = (i + 1 < onsets.size()) ? onsets[i + 1].TimestampMs : (onset.TimestampMs + maxHoldMs);
                    double sustainLimitMs = std::min(onset.TimestampMs + maxHoldMs, nextOnsetMs - 10.0);

                    double sustainEndMs = onset.TimestampMs;
                    for (size_t j = frameIdx + 1; j < timeline.size() && timeline[j].TimestampMs <= sustainLimitMs; ++j) {
                        float energy = timeline[j].SubBassEnergy + timeline[j].MidEnergy + timeline[j].HighEnergy;
                        if (baseline <= 0.0f || energy < baseline * 0.5f) break;
                        sustainEndMs = timeline[j].TimestampMs;
                    }

                    if (!suppressComplexity && sustainEndMs - onset.TimestampMs >= minHoldMs) {
                        releaseTimeMs = static_cast<int>(sustainEndMs);
                    }
                }
            }

            notes.push_back(PlacedNote{ mainLane, hitTimeMs, releaseTimeMs });

            if (wantsJump) {
                int secondLane = _pickLane(mainLane, onset.SubBassRatio, onset.HighRatio);
                if (secondLane == mainLane && Config.Keys > 1) {
                    secondLane = (secondLane + 1) % static_cast<int>(Config.Keys);
                }
                notes.push_back(PlacedNote{ secondLane, hitTimeMs, -1 });
            }

            previousLane = mainLane;
            previousTimestampMs = onset.TimestampMs;
        }

        std::stable_sort(notes.begin(), notes.end(), [](const PlacedNote& a, const PlacedNote& b) {
            return a.HitTimeMs < b.HitTimeMs;
        });

        return notes;
    }

    double PatternLibrary::_rateNotes(const std::vector<PlacedNote>& notes) const {
        if (notes.empty()) return 0.0;

        eppyphany::Generation::dotosuFileConfig scratchConfig{};
        scratchConfig.Keys = Config.Keys;
        scratchConfig.BPM = 120;

        eppyphany::Generation::dotosu scratchFile(scratchConfig);
        for (const auto& note : notes) {
            scratchFile.AddHitObject(note.Column, note.HitTimeMs, note.ReleaseTimeMs);
        }

        eppyphany::Difficulty::ManiaDifficultyCalculator calculator;
        return calculator.Calculate(scratchFile);
    }

    std::vector<PlacedNote> PatternLibrary::Generate(const std::vector<AudioFrame>& timeline) {
        if (timeline.empty()) return {};

        float lowMultiplier = 0.3f;
        float highMultiplier = 4.0f;

        float bestMultiplier = (lowMultiplier + highMultiplier) / 2.0f;
        double bestDiff = std::numeric_limits<double>::max();
        double bestSr = 0.0;

        std::mt19937 rngSnapshot = rng_;

        const int maxIterations = 10;
        for (int iter = 0; iter < maxIterations; ++iter) {
            float midMultiplier = (lowMultiplier + highMultiplier) / 2.0f;

            auto onsets = _detectOnsets(timeline, midMultiplier);
            _computeAuralFeatures(onsets);

            rng_ = rngSnapshot; // deterministic replay for this threshold
            auto notes = _assignPatterns(onsets, timeline);

            double estimatedSr = _rateNotes(notes);

            double diff = estimatedSr - Config.TargetStarRating;
            if (std::abs(diff) < std::abs(bestDiff)) {
                bestDiff = diff;
                bestMultiplier = midMultiplier;
                bestSr = estimatedSr;
            }

            if (estimatedSr > Config.TargetStarRating) {
                lowMultiplier = midMultiplier;
            } else {
                highMultiplier = midMultiplier;
            }

            if (std::abs(diff) < 0.1) break;
        }

        auto finalOnsets = _detectOnsets(timeline, bestMultiplier);
        _computeAuralFeatures(finalOnsets);
        rng_ = rngSnapshot;
        auto notes = _assignPatterns(finalOnsets, timeline);

        lastThresholdMultiplier_ = bestMultiplier;
        lastEstimatedSr_ = bestSr;

        return notes;
    }

    std::vector<KiaiSection> PatternLibrary::DetectKiaiSections(const std::vector<AudioFrame>& timeline) const {
        std::vector<KiaiSection> sections;
        if (timeline.size() < 3) return sections;

        const double sectionWindowMs = 4000.0; // ~4s rolling window
        const double minKiaiDurationMs = 4000.0; // don't bother with blip-length kiai
        const double mergeGapMs = 2000.0; // merge kiai sections separated by a short dip

        std::vector<double> intensity(timeline.size());
        for (size_t i = 0; i < timeline.size(); ++i) {
            intensity[i] = timeline[i].SubBassEnergy + timeline[i].MidEnergy +
                           timeline[i].HighEnergy + timeline[i].TotalFlux;
        }

        double songMean = 0.0;
        for (double v : intensity) songMean += v;
        songMean /= intensity.size();

        double songVariance = 0.0;
        for (double v : intensity) songVariance += (v - songMean) * (v - songMean);
        songVariance /= intensity.size();
        double songStddev = std::sqrt(songVariance);

        if (songStddev <= 1e-9) return sections; // flat/silent track, nothing to mark

        double kiaiThreshold = songMean + 0.5 * songStddev;

        std::vector<bool> aboveThreshold(timeline.size(), false);
        for (size_t i = 0; i < timeline.size(); ++i) {
            double windowStart = timeline[i].TimestampMs - sectionWindowMs / 2.0;
            double windowEnd = timeline[i].TimestampMs + sectionWindowMs / 2.0;

            double sum = 0.0;
            int count = 0;
            for (size_t j = i;; ) {
                if (timeline[j].TimestampMs < windowStart) break;
                sum += intensity[j];
                ++count;
                if (j == 0) break;
                --j;
            }
            for (size_t j = i + 1; j < timeline.size() && timeline[j].TimestampMs <= windowEnd; ++j) {
                sum += intensity[j];
                ++count;
            }

            double rollingAvg = (count > 0) ? (sum / count) : 0.0;
            aboveThreshold[i] = rollingAvg > kiaiThreshold;
        }

        size_t i = 0;
        while (i < aboveThreshold.size()) {
            if (!aboveThreshold[i]) { ++i; continue; }

            size_t sectionStart = i;
            while (i < aboveThreshold.size() && aboveThreshold[i]) ++i;
            size_t sectionEnd = i - 1;

            int startMs = static_cast<int>(timeline[sectionStart].TimestampMs);
            int endMs = static_cast<int>(timeline[sectionEnd].TimestampMs);

            if (!sections.empty() && (startMs - sections.back().EndMs) <= static_cast<int>(mergeGapMs)) {
                sections.back().EndMs = endMs;
            } else if (endMs - startMs >= static_cast<int>(minKiaiDurationMs)) {
                sections.push_back(KiaiSection{ startMs, endMs });
            }
        }

        return sections;
    }

}