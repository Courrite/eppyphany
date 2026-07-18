#include "Generation/PatternLibrary.hpp"
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

        // rolling mean/stddev window, in time rather than frame count, since
        // hop size (and therefore frames-per-second) isn't known here
        const double windowMs = 1000.0;
        const double minGapMs = 45.0;

        float peakFlux = 0.0f;
        for (const auto& f : timeline) peakFlux = std::max(peakFlux, f.TotalFlux);
        if (peakFlux <= 0.0f) return onsets; // silence, or the FFT stub is still stubbed out somewhere

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
            if (count < 4) continue; // not enough history yet to judge a real spike vs. noise

            float mean = sum / count;
            float variance = (sumSq / count) - (mean * mean);
            float stddev = variance > 0.0f ? std::sqrt(variance) : 0.0f;
            if (stddev <= 1e-6f) continue; // flat window - no meaningful peak to score

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

        std::vector<double> acceptedTimestamps; // kept sorted chronologically
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
            // re-locate the frame for this timestamp
            while (candidateFrame < timeline.size() && timeline[candidateFrame].TimestampMs < ts) ++candidateFrame;
            if (candidateFrame >= timeline.size()) break;
            const auto& cur = timeline[candidateFrame];

            float bandTotal = cur.SubBassEnergy + cur.MidEnergy + cur.HighEnergy;
            Onset o;
            o.TimestampMs = cur.TimestampMs;
            o.Strength = std::min(1.0f, cur.TotalFlux / peakFlux);
            if (bandTotal > 0.0f) {
                o.SubBassRatio = cur.SubBassEnergy / bandTotal;
                o.MidRatio = cur.MidEnergy / bandTotal;
                o.HighRatio = cur.HighEnergy / bandTotal;
            } else {
                o.SubBassRatio = o.MidRatio = o.HighRatio = 1.0f / 3.0f;
            }
            onsets.push_back(o);
        }

        return onsets;
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

            bool wantsJack = previousLane >= 0 &&
                              gapToPrev < jackWindowMs &&
                              onset.Strength > 0.5f &&
                              chance(rng_) < 0.35f;

            bool wantsJump = !wantsJack &&
                              onset.Strength > 0.75f &&
                              Config.Keys >= 2 &&
                              chance(rng_) < 0.30f;

            int mainLane = wantsJack ? previousLane : _pickLane(previousLane, onset.SubBassRatio, onset.HighRatio);
            int hitTimeMs = static_cast<int>(onset.TimestampMs);

            // decide whether this note should become a hold: does energy
            // stay elevated past this onset instead of decaying right away?
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

                    if (sustainEndMs - onset.TimestampMs >= minHoldMs) {
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

    float PatternLibrary::_estimateStarRatingFromOnsets(const std::vector<Onset>& onsets, double songDurationMs) const {
        if (onsets.empty() || songDurationMs <= 0.0) return 0.0f;

        float onsetsPerSecond = static_cast<float>(onsets.size()) / static_cast<float>(songDurationMs / 1000.0);
        float avgStrength = 0.0f;
        for (const auto& o : onsets) avgStrength += o.Strength;
        avgStrength /= onsets.size();

        return 1.0f + 0.9f * onsetsPerSecond + 1.0f * avgStrength;
    }

    std::vector<PlacedNote> PatternLibrary::Generate(const std::vector<AudioFrame>& timeline) {
        if (timeline.empty()) return {};

        double songDurationMs = timeline.back().TimestampMs;

        float lowMultiplier = 0.3f; // aggressive: catches lots of onsets, likely too hard
        float highMultiplier = 4.0f; // conservative: only the biggest spikes, likely too easy

        float bestMultiplier = (lowMultiplier + highMultiplier) / 2.0f;
        float bestDiff = std::numeric_limits<float>::max();

        const int maxIterations = 10;
        for (int iter = 0; iter < maxIterations; ++iter) {
            float midMultiplier = (lowMultiplier + highMultiplier) / 2.0f;

            auto onsets = _detectOnsets(timeline, midMultiplier);
            float estimatedSr = _estimateStarRatingFromOnsets(onsets, songDurationMs);

            float diff = estimatedSr - Config.TargetStarRating;
            if (std::abs(diff) < std::abs(bestDiff)) {
                bestDiff = diff;
                bestMultiplier = midMultiplier;
            }

            if (estimatedSr > Config.TargetStarRating) {
                lowMultiplier = midMultiplier;
            } else {
                highMultiplier = midMultiplier;
            }

            if (std::abs(diff) < 0.1f) break; // close enough, stop early
        }

        auto finalOnsets = _detectOnsets(timeline, bestMultiplier);
        auto notes = _assignPatterns(finalOnsets, timeline);

        lastThresholdMultiplier_ = bestMultiplier;

        return notes;
    }

}