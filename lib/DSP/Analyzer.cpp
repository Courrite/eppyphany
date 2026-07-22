#include "DSP/Analyzer.hpp"
#include <cmath>
#include <complex>
#include <vector>
#include <algorithm>
#include "DSP/AudioLoader.hpp"
#include "DSP/Cepstrum.hpp"
#include <iostream>

extern "C" {
    #include <kissfft/kiss_fftr.h>
}

namespace eppyphany::DSP {
    Analyzer::Analyzer(int fftSize, int hopSize):
    fftSize_(fftSize),
    hopSize_(hopSize) {
        if (fftSize_ % 2 != 0) {
            std::cerr << "FFT size must be even for a real-input FFT, rounding " << fftSize_ << " down to " << (fftSize_ - 1) << ".\n";
            fftSize_ -= 1;
        }
    };

    std::vector<double> Analyzer::_magnitude(const std::vector<std::complex<double>>& fft) {
        std::vector<double> mag(fft.size());
        for (int i = 0; i < fft.size(); ++i)
            mag[i] = std::abs(fft[i]);
        return mag;
    }

    std::vector<double> Analyzer::_computeChroma(const std::vector<double>& mag, double sampleRate) {
        std::vector<double> chroma(12, 0.0);
        double binWidth = sampleRate / fftSize_;
        for (int b = 1; b < mag.size(); ++b) {
            double freq = b * binWidth;
            if (freq < 20.0) continue;
            
            double midi = 12.0 * std::log2(freq / 440.0) + 69.0;
            int pc = std::lround(midi) % 12;
            if (pc < 0) pc += 12;
            chroma[pc] += mag[b];
        }

        double total = 0.0;
        for (double v : chroma)
            total += v;

        if (total > 1e-9)
            for (auto& v : chroma)
                v /= total;

        return chroma;
    }

    double Analyzer::_calculateSpectralFlux(const std::vector<double>& cur, const std::vector<double>& prev) {
        double flux = 0.0;
        for (int i = 0; i < cur.size(); ++i) {
            double diff = cur[i] - prev[i];
            if (diff > 0.0) flux += diff;
        }
        return flux;
    }

    std::vector<AudioFrame> Analyzer::Analyze(const Audio& audio) {
        std::vector<AudioFrame> timeline;
        if (audio.Samples.empty()) return timeline;

        std::vector<std::vector<double>> spectra;
        std::vector<double> prevMag(fftSize_ / 2 + 1, 0.0);
        std::vector<double> prevChroma(12, 0.0);
        std::vector<double> frame(fftSize_, 0.0);

        std::vector<double> cepstrum(fftSize_);
        Cepstrum ceps(fftSize_);

        for (int offset = 0; offset + fftSize_ < audio.Samples.size(); offset += hopSize_) {
            std::copy(audio.Samples.begin() + offset, audio.Samples.begin() + offset + fftSize_, frame.begin());
            double timestampMs = (static_cast<double>(offset) / audio.SampleRate) * 1000.0;

            ceps.ApplyHannWindow(frame);
            
            auto fftData = ceps.ComputeFFT(frame);
            auto mag = _magnitude(fftData);
            auto chroma = _computeChroma(mag, audio.SampleRate);

            ceps.Forward(frame, cepstrum);
            
            double percussiveEnergy = ceps.PercussiveEnergy(cepstrum, 2, 10);

            AudioFrame f{};
            f.TimestampMs = timestampMs;
            f.TotalFlux = _calculateSpectralFlux(mag, prevMag);
            f.ChromaFlux = _calculateSpectralFlux(chroma, prevChroma);
            f.PercussiveCepstralEnergy = percussiveEnergy;

            timeline.push_back(f);
            spectra.push_back(mag);
            prevMag = mag;
            prevChroma = chroma;
        }

        _computeAttackSharpness(timeline, spectra);
        _computeSectionNovelty(timeline, spectra);
        _computeSpectralShape(timeline, spectra, audio.SampleRate);
        
        return timeline;
    }

    void Analyzer::_computeAttackSharpness(std::vector<AudioFrame>& timeline, const std::vector<std::vector<double>>& spectra) {
        const int lookahead = 6;
        for (int i = 0; i < timeline.size(); ++i) {
            const auto& cur = spectra[i];
            double curTotal = 0.0, nextTotal = 0.0;
            int count = 0;

            for (int j = i + 1; j < spectra.size() && j <= i + lookahead; ++j) {
                for (double v : spectra[j]) nextTotal += v;
                ++count;
            }
            
            if (count > 0)
                nextTotal /= static_cast<double>(count);

            for (double v : cur) curTotal += v;

            double denom = curTotal + nextTotal;
            timeline[i].AttackSharpness = denom > 1e-9
                ? std::clamp((curTotal - nextTotal) / denom, 0.0, 1.0)
                : 0.0;
        }
    }

    void Analyzer::_computeSectionNovelty(std::vector<AudioFrame>& timeline, const std::vector<std::vector<double>>& spectra) {
        if (timeline.empty()) return;
        int binCount = spectra[0].size();

        std::vector<Block> blocks;
        const double blockMs = 1000.0;

        int frameIdx = 0;
        while (frameIdx < timeline.size()) {
            double blockStart = timeline[frameIdx].TimestampMs;
            Block b;
            b.Sum.assign(binCount, 0.0);
            b.FirstFrame = frameIdx;

            while (frameIdx < timeline.size() && timeline[frameIdx].TimestampMs < blockStart + blockMs) {
                for (int bin = 0; bin < binCount; ++bin)
                    b.Sum[bin] += spectra[frameIdx][bin];

                ++frameIdx;
            }

            b.LastFrame = frameIdx - 1;
            blocks.push_back(std::move(b));
        }

        auto normalize = [](std::vector<double>& v) {
            double norm = 0.0;
            for (double x : v)
                norm += x * x;

            norm = std::sqrt(norm);
            if (norm > 1e-9) for (auto& x : v) x /= norm;
        };

        const int lookbackBlocks = 6;
        std::vector<double> blockNovelty(blocks.size(), 0.0);

        for (int bi = 0; bi < blocks.size(); ++bi) {
            int histStart = (bi >= lookbackBlocks) ? bi - lookbackBlocks : 0;
            if (histStart == bi) continue;

            std::vector<double> hist(binCount, 0.0);

            for (int hj = histStart; hj < bi; ++hj)
                for (int bin = 0; bin < binCount; ++bin)
                    hist[bin] += blocks[hj].Sum[bin];

            std::vector<double> cur = blocks[bi].Sum;
            normalize(cur);
            normalize(hist);

            double cosineSim = 0.0;
            for (int bin = 0; bin < binCount; ++bin)
                cosineSim += cur[bin] * hist[bin];

            blockNovelty[bi] = std::clamp(1.0 - cosineSim, 0.0, 1.0);
        }

        for (int bi = 0; bi < blocks.size(); ++bi)
            for (int fi = blocks[bi].FirstFrame; fi <= blocks[bi].LastFrame && fi < timeline.size(); ++fi)
                timeline[fi].SectionNovelty = blockNovelty[bi];
    }

    void Analyzer::_computeSpectralShape(std::vector<AudioFrame>& timeline, const std::vector<std::vector<double>>& spectra, double sampleRate) {
        if (spectra.empty()) return;
        double binWidth = sampleRate / fftSize_;
        double nyquist = sampleRate / 2.0;

        for (int i = 0; i < timeline.size(); ++i) {
            const auto& mag = spectra[i];
            double weightedSum = 0.0, total = 0.0;
            for (int b = 0; b < mag.size(); ++b) {
                double freq = b * binWidth;
                weightedSum += freq * mag[b];
                total += mag[b];
            }

            if (total <= 1e-9) {
                timeline[i].Brightness = 0.0;
                timeline[i].Rolloff = 0.0;
                continue;
            }

            timeline[i].Brightness = std::clamp((weightedSum / total) / nyquist, 0.0, 1.0);

            double target = total * 0.85;
            double running = 0.0;
            double rolloffFreq = nyquist;
            for (int b = 0; b < mag.size(); ++b) {
                running += mag[b];
                if (running >= target) { rolloffFreq = b * binWidth; break; }
            }
            timeline[i].Rolloff = std::clamp(rolloffFreq / nyquist, 0.0, 1.0);
        }
    }
}