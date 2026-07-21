#include "DSP/Separator.hpp"
#include <cmath>
#include <algorithm>

namespace eppyphany::DSP {
    Separator::Separator(int fftSize, int hopSize)
        : fftSize_(fftSize), hopSize_(hopSize), ceps_(fftSize) {}

    Separator::~Separator() {}

    double Separator::_weight(double confidence, double k, double n) {
        double cn = std::pow(confidence, n);
        double kn = std::pow(k, n);
        return cn / (cn + kn); 
    }

    Audio Separator::Isolate(const Audio& audio, double minF0, double maxF0, double k) {
        Audio result;
        result.SampleRate = audio.SampleRate;
        result.Samples.assign(audio.Samples.size(), 0.0);
        if (audio.Samples.empty()) return result;

        std::vector<double> weightAccum(audio.Samples.size(), 0.0);

        std::vector<double> window(fftSize_, 1.0);
        ceps_.ApplyHannWindow(window);

        std::vector<double> frame(fftSize_, 0.0);
        std::vector<double> cepstrum(fftSize_);
        std::vector<double> mask(fftSize_);
        std::vector<double> isolated(fftSize_);

        for (int offset = 0; offset + fftSize_ < audio.Samples.size(); offset += hopSize_) {
            std::copy(audio.Samples.begin() + offset, audio.Samples.begin() + offset + fftSize_, frame.begin());
            ceps_.ApplyHannWindow(frame);

            ceps_.Forward(frame, cepstrum);
            F0Estimate est = ceps_.EstimateF0(cepstrum, audio.SampleRate, minF0, maxF0);

            double weight = 0.0;
            if (est.Frequency > 0.0) {
                weight = _weight(est.Confidence, k);
                ceps_.BuildCombMask(est.Frequency, audio.SampleRate, mask);
                ceps_.ApplyMask(cepstrum, mask);
                ceps_.Inverse(cepstrum, isolated);
            } else {
                std::fill(isolated.begin(), isolated.end(), 0.0);
            }

            for (int i = 0; i < fftSize_; ++i) {
                double blended = weight * isolated[i] + (1.0 - weight) * frame[i];
                result.Samples[offset + i] += blended;
                weightAccum[offset + i] += window[i];
            }
        }

        for (int i = 0; i < result.Samples.size(); ++i) {
            if (weightAccum[i] > 1e-9) result.Samples[i] /= weightAccum[i];
        }

        return result;
    }
}