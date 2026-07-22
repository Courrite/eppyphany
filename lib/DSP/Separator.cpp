#include "DSP/Separator.hpp"
#include "Utils/MathUtils.hpp"
#include <cmath>
#include <algorithm>
#include <complex>

using namespace eppyphany::Utils;

namespace eppyphany::DSP {
    Separator::Separator(int fftSize, int hopSize) : fftSize_(fftSize), hopSize_(hopSize), ceps_(fftSize) {}

    double Separator::_weight(double confidence, double k, double n) {
        double cn = std::pow(confidence, n);
        double kn = std::pow(k, n);
        return cn / (cn + kn); 
    }

    std::vector<double> Separator::_calculatePowerSpectrum(const std::vector<std::complex<double>>& fft) {
        const int numUniqueBins = (fft.size() / 2) + 1;
        std::vector<double> powerSpectrum(numUniqueBins);

        for (int i = 0; i < numUniqueBins; ++i) {
            powerSpectrum[i] = std::norm(fft[i]);
        }

        return powerSpectrum;
    }

    double Separator::_calculateSFM(const std::vector<double>& powerSpectrum) {
        const int M = powerSpectrum.size();
        if (M == 0) return 1.0;

        const double eps = 1e-12;
        double natural_p_sum = 0.0;
        double p_sum = 0.0;

        for (auto x : powerSpectrum) {
            natural_p_sum += MathUtils::ln(x + eps);
            p_sum += x;
        }

        double u_value = std::exp(1.0 / static_cast<double>(M) * natural_p_sum);
        double l_value = 1.0 / static_cast<double>(M) * p_sum;

        if (l_value < eps) return 1.0; // silence is treated as flat noise

        return u_value / l_value;
    }

    bool Separator::_isResidualWhiteNoise(double sfm, int M) const {
        if (M == 0) return true;

        const double eps = 1e-12;
        double clampedSFM = std::clamp(sfm, eps, 1.0);

        const double gamma = 0.57721566490153286; 
        const double expectedMeanLogSFM = -gamma;
        const double varianceLogSFM = (M_PI * M_PI) / (6.0 * static_cast<double>(M));
        const double stdDevLogSFM = std::sqrt(varianceLogSFM);

        double zScore = (std::log(clampedSFM) - expectedMeanLogSFM) / stdDevLogSFM;

        return zScore > -2.0;
    }

    double Separator::_calculateSpectralEntropy(const std::vector<double>& powerSpectrum) const {
        const int M = powerSpectrum.size();
        if (M <= 1) return 1.0;

        double sumPower = 0.0;
        for (double p : powerSpectrum) sumPower += p;

        const double eps = 1e-12;
        if (sumPower < eps) return 1.0;

        double entropy = 0.0;
        for (double p : powerSpectrum) {
            double prob = p / sumPower;
            if (prob > eps) {
                entropy -= prob * std::log(prob);
            }
        }

        double maxEntropy = std::log(static_cast<double>(M));
        return std::clamp(entropy / maxEntropy, 0.0, 1.0);
    }

    bool Separator::_isPeakSignificant(const std::vector<double>& cepstrum, int minLag, int maxLag, int peakLag) const {
        if (minLag >= maxLag || peakLag < minLag || peakLag > maxLag) return false;

        double sum = 0.0;
        int count = maxLag - minLag + 1;

        for (int i = minLag; i <= maxLag; ++i) {
            sum += cepstrum[i];
        }
        double mean = sum / static_cast<double>(count);

        double varianceSum = 0.0;
        for (int i = minLag; i <= maxLag; ++i) {
            double diff = cepstrum[i] - mean;
            varianceSum += diff * diff;
        }
        double stdDev = std::sqrt(varianceSum / static_cast<double>(count));

        if (stdDev < 1e-12) return false;

        double zScore = (cepstrum[peakLag] - mean) / stdDev;
        const double criticalZ = 1.6448536269514722;

        return zScore >= criticalZ;
    }

    std::vector<Audio> Separator::Isolate(const Audio& audio, double minF0, double maxF0, double k) {
        if (audio.Samples.empty()) return {};

        const int totalSamples = static_cast<int>(audio.Samples.size());

        // stem 0: pitch isolated component
        // stem 1: residual or background
        std::vector<std::vector<double>> layerOutputs(2, std::vector<double>(totalSamples, 0.0));
        std::vector<double> windowNormAccum(totalSamples, 0.0);

        // precompute hann window
        std::vector<double> window(fftSize_, 1.0);
        ceps_.ApplyHannWindow(window);

        std::vector<double> frame(fftSize_);
        std::vector<double> cepstrum(fftSize_);
        std::vector<double> mask(fftSize_);
        std::vector<double> isolated(fftSize_);

        const int minLag = static_cast<int>(audio.SampleRate / maxF0);
        const int maxLag = static_cast<int>(audio.SampleRate / minF0);

        for (int offset = 0; offset + fftSize_ <= totalSamples; offset += hopSize_) {
            for (int i = 0; i < fftSize_; ++i) {
                frame[i] = audio.Samples[offset + i] * window[i];
            }

            ceps_.Forward(frame, cepstrum);
            F0Estimate est = ceps_.EstimateF0(cepstrum, audio.SampleRate, minF0, maxF0);

            bool isValidPitch = false;
            double weight = 0.0;

            if (est.Frequency > 0.0) {
                int peakLag = static_cast<int>(audio.SampleRate / est.Frequency);
                if (_isPeakSignificant(cepstrum, minLag, maxLag, peakLag)) {
                    isValidPitch = true;
                    weight = _weight(est.Confidence, k);
                }
            }

            if (isValidPitch && weight > 1e-4) {
                ceps_.BuildCombMask(est.Frequency, audio.SampleRate, mask);
                ceps_.ApplyMask(cepstrum, mask);
                ceps_.Inverse(cepstrum, isolated);

                for (int i = 0; i < fftSize_; ++i) {
                    double isoSample = isolated[i] * weight * window[i];
                    double origSample = frame[i];

                    layerOutputs[0][offset + i] += isoSample;
                    layerOutputs[1][offset + i] += (origSample - isoSample);
                }
            } else {
                for (int i = 0; i < fftSize_; ++i) {
                    layerOutputs[1][offset + i] += frame[i];
                }
            }

            // accumulate window normalization factor
            for (int i = 0; i < fftSize_; ++i) {
                windowNormAccum[offset + i] += window[i] * window[i];
            }
        }

        std::vector<Audio> stems(2);
        stems[0].SampleRate = audio.SampleRate;
        stems[0].Samples.assign(totalSamples, 0.0);
        stems[1].SampleRate = audio.SampleRate;
        stems[1].Samples.assign(totalSamples, 0.0);

        for (int i = 0; i < totalSamples; ++i) {
            if (windowNormAccum[i] > 1e-9) {
                stems[0].Samples[i] = layerOutputs[0][i] / windowNormAccum[i];
                stems[1].Samples[i] = layerOutputs[1][i] / windowNormAccum[i];
            }
        }

        return stems;
    }
}