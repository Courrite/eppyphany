#include "Generation/Composer.hpp"
#include "DSP/AudioLoader.hpp"
#include <algorithm>
#include <cmath>

using namespace eppyphany::DSP;

namespace eppyphany::Generation {
    namespace {
        // applies one onset placement to the state, updates timing/column entropy in place.
        void ApplyOnset(ColumnState& s, int column, double timestampMs, int totalColumns) {
            double gap = timestampMs - s.LastHit[column];
            
            const double alpha = 0.2; // EMA smoothing
            if (s.LastHit[column] >= 0 && s.AverageGap[column] > 0.0) {
                double relDeviation = std::abs(gap - s.AverageGap[column]) / s.AverageGap[column];
                s.TimingEntropy[column] = (1.0 - alpha) * s.TimingEntropy[column] + alpha * relDeviation;
                s.AverageGap[column] = (1.0 - alpha) * s.AverageGap[column] + alpha * gap;
            } else if (gap > 0) {
                s.AverageGap[column] = gap;
            }

            s.LastHit[column] = timestampMs;
            s.HitCounts[column]++;

            int handSplit = totalColumns / 2;
            for (int hand = 0; hand < 2; ++hand) {
                int lo = hand == 0 ? 0 : handSplit;
                int hi = hand == 0 ? handSplit : totalColumns;

                int total = 0;
                for (int c = lo; c < hi; ++c) total += s.HitCounts[c];
                if (total == 0) {
                    s.ColumnEntropy[hand] = 0.0;
                    continue;
                }
                
                double entropy = 0.0;
                for (int c = lo; c < hi; ++c) {
                    if (s.HitCounts[c] == 0) continue;
                    double p = static_cast<double>(s.HitCounts[c]) / total;
                    entropy -= p * std::log2(p);
                }
                s.ColumnEntropy[hand] = entropy;
            }
        }

        // cost of placing an onset at column given the state as it stands before placement
        double StepCost(const ColumnState& s, int column, double timestampMs, double optimalGapMs) {
            if (s.LastHit[column] < 0) 
                return 0.0; // first hit in this column, free

            double gap = timestampMs - s.LastHit[column];
            double gapCost = std::abs(gap - optimalGapMs) / std::max(optimalGapMs, 1.0);
            double recencyCost = gap < optimalGapMs * 0.25 ? 1.0 : 0.0; // near-instant repeat, heavy penalty
            double entropyCost = s.TimingEntropy[column] * 0.3;

            return gapCost + recencyCost + entropyCost;
        }
    }

    Composer::Composer(int fftSize, int hopSize)
        : fftSize_(fftSize), hopSize_(hopSize),
          analyzer_(fftSize, hopSize), separator_(fftSize, hopSize) {}

    double Composer::_getOptimalTimingGap(double sr) {
        // f(SR) = 1 / sqrt(SR ^ sqrt(1.5*pi)), expected min gap in seconds, soft cost basis
        double exponent = std::sqrt(1.5 * M_PI);
        return 1.0 / std::sqrt(std::pow(sr, exponent));
    }

    std::vector<OnsetCandidate> Composer::_pickOnsets(const std::vector<AudioFrame>& timeline, int stemIndex) {
        std::vector<OnsetCandidate> candidates;
        if (timeline.size() < 3) return candidates;

        std::vector<double> score(timeline.size());
        for (int i = 0; i < timeline.size(); ++i)
            score[i] = timeline[i].TotalFlux + timeline[i].ChromaFlux + timeline[i].PercussiveCepstralEnergy;

        const int win = 43; // ~1s of context either side at typical hop/samplerate
        const double k = 1.5; // noise-floor multiplier on local stddev

        for (int i = 1; i + 1 < score.size(); ++i) {
            if (score[i] <= score[i - 1] || score[i] < score[i + 1])
                continue; // not a local peak

            int lo = i > win ? i - win : 0;
            int hi = std::min<int>(score.size(), i + win);

            double sum = 0.0, sumSq = 0.0;
            int n = hi - lo;
            for (int j = lo; j < hi; ++j) {
                sum += score[j];
                sumSq += score[j] * score[j];
            }

            double mean = sum / n;
            double variance = sumSq / n - mean * mean;
            double stddev = variance > 0 ? std::sqrt(variance) : 0.0;
            double floor = mean + k * stddev;

            if (score[i] > floor) {
                candidates.push_back({timeline[i].TimestampMs, score[i], stemIndex});
            }
        }
        return candidates;
    }

    std::vector<OnsetCandidate> Composer::_buildCandidatePool(const Audio& audio) {
        std::vector<OnsetCandidate> pool;

        auto mixTimeline = analyzer_.Analyze(audio);
        auto mixCandidates = _pickOnsets(mixTimeline, 0);
        pool.insert(pool.end(), mixCandidates.begin(), mixCandidates.end());

        std::vector<Audio> stems = separator_.Isolate(audio, 80.0, 1200.0, 2.0);

        for (size_t s = 0; s < stems.size(); ++s) {
            auto stemTimeline = analyzer_.Analyze(stems[s]);
            auto stemCandidates = _pickOnsets(stemTimeline, static_cast<int>(s + 1));
            pool.insert(pool.end(), stemCandidates.begin(), stemCandidates.end());
        }

        std::sort(pool.begin(), pool.end(), [](const auto& a, const auto& b) { 
            return a.TimestampMs < b.TimestampMs; 
        });

        return pool;
    }

    void Composer::_updateEntropy(int column, double timestampMs, int totalColumns) {
        ColumnState s(totalColumns);
        s.LastHit = lastHitInColumn_;
        s.AverageGap = avgGapInColumn_;
        s.TimingEntropy = timingEntropy_;
        s.HitCounts = columnHitCounts_;
        s.ColumnEntropy = columnEntropy_;

        ApplyOnset(s, column, timestampMs, totalColumns);

        lastHitInColumn_ = s.LastHit;
        avgGapInColumn_ = s.AverageGap;
        timingEntropy_ = s.TimingEntropy;
        columnHitCounts_ = s.HitCounts;
        columnEntropy_ = s.ColumnEntropy;
    }

    std::vector<HitObject> Composer::_beamSearch(const std::vector<OnsetCandidate>& candidates, const MapConfig& config, uint64_t availablePatterns) {
        // TODO: pattern-family logic not yet threaded through this pass

        std::vector<HitObject> result;
        int totalColumns = config.Keys;
        if (totalColumns == 0 || candidates.empty())
            return result;

        lastHitInColumn_.assign(totalColumns, -1);
        avgGapInColumn_.assign(totalColumns, 0.0);
        timingEntropy_.assign(totalColumns, 0.0);
        columnHitCounts_.assign(totalColumns, 0);
        columnEntropy_.assign(2, 0.0);

        double optimalGapMs = _getOptimalTimingGap(config.StarRating) * 1000.0;

        const int windowSize = 6;
        const int beamWidth = 8;

        int cursor = 0;
        while (cursor < candidates.size()) {
            int windowEnd = std::min<int>(candidates.size(), cursor + windowSize);

            ColumnState masterState(totalColumns);
            masterState.LastHit = lastHitInColumn_;
            masterState.AverageGap = avgGapInColumn_;
            masterState.TimingEntropy = timingEntropy_;
            masterState.HitCounts = columnHitCounts_;
            masterState.ColumnEntropy = columnEntropy_;

            std::vector<BeamPath> beam;
            BeamPath initialPath{masterState, std::vector<int>{}, 0.0};
            beam.push_back(std::move(initialPath));

            for (int i = cursor; i < windowEnd; ++i) {
                double ts = candidates[i].TimestampMs;
                std::vector<BeamPath> nextBeam;

                for (const auto& path : beam) {
                    for (int c = 0; c < totalColumns; ++c) {
                        BeamPath expanded{path.State, path.ChosenColumns, path.Cost};
                        double stepCost = StepCost(expanded.State, c, ts, optimalGapMs);

                        ApplyOnset(expanded.State, c, ts, totalColumns);

                        expanded.ChosenColumns.push_back(c);
                        expanded.Cost += stepCost;

                        nextBeam.push_back(std::move(expanded));
                    }
                }

                std::sort(nextBeam.begin(), nextBeam.end(), [](const auto& a, const auto& b) { return a.Cost < b.Cost; });
                if (nextBeam.size() > beamWidth) {
                    nextBeam.erase(nextBeam.begin() + beamWidth, nextBeam.end());
                }
                beam = std::move(nextBeam);
            }

            // add only the first candidate's chosen column from the best surviving path.
            const auto& best = beam.front();
            int chosenColumn = best.ChosenColumns.front();
            double ts = candidates[cursor].TimestampMs;

            HitObject obj;
            obj.Column = chosenColumn + 1; // dotosu columns are 1-indexed
            obj.HitTime = ts;
            obj.Type = HitObjectType::Note; // ln pairing not yet implemented in this pass
            result.push_back(obj);

            _updateEntropy(chosenColumn, ts, totalColumns);

            ++cursor;
        }

        return result;
    }

    std::vector<ReviewFlag> Composer::_review(const std::vector<HitObject>& hitObjects, int totalColumns) {
        std::vector<ReviewFlag> flags;
        if (hitObjects.empty() || totalColumns == 0) return flags;

        const double blockMs = 1000.0;
        int start = hitObjects.front().HitTime;
        int end = hitObjects.back().HitTime;

        for (double blockStart = start; blockStart <= end; blockStart += blockMs) {
            double blockEnd = blockStart + blockMs;

            std::vector<double> LastHit(totalColumns, -1.0);
            std::vector<int> HitCounts(totalColumns, 0);
            std::vector<double> gapDeviation(totalColumns, 0.0);

            for (const auto& obj : hitObjects) {
                if (obj.HitTime < blockStart || obj.HitTime >= blockEnd) continue;
                int col = obj.Column - 1;
                if (col < 0 || col >= totalColumns) continue;

                HitCounts[col]++;
                if (LastHit[col] >= 0) {
                    double gap = obj.HitTime - LastHit[col];
                    gapDeviation[col] = std::max(gapDeviation[col], 1.0 / std::max(gap, 1.0));
                }
                LastHit[col] = obj.HitTime;
            }

            double timingScore = 0.0;
            for (double d : gapDeviation)
                timingScore = std::max(timingScore, d);

            int handSplit = totalColumns / 2;
            double columnScore = 0.0;
            for (int hand = 0; hand < 2; ++hand) {
                int lo = hand == 0 ? 0 : handSplit;
                int hi = hand == 0 ? handSplit : totalColumns;

                int total = 0;
                for (int c = lo; c < hi; ++c)
                    total += HitCounts[c];

                if (total == 0)
                    continue;

                double entropy = 0.0;
                for (int c = lo; c < hi; ++c) {
                    if (HitCounts[c] == 0) continue;
                    double p = static_cast<double>(HitCounts[c]) / total;
                    entropy -= p * std::log2(p);
                }
                columnScore += entropy;
            }

            flags.push_back({blockStart, blockEnd, timingScore, columnScore});
        }

        return flags;
    }

    std::vector<HitObject> Composer::Compose(const MapConfig& config, Audio decoded, uint64_t availablePatterns) {
        auto candidates = _buildCandidatePool(decoded);
        auto hitObjects = _beamSearch(candidates, config, availablePatterns);

        auto flags = _review(hitObjects, config.Keys);

        // TODO: nothing consumes review flags yet, no regeneration hook wired up

        return hitObjects;
    }
}