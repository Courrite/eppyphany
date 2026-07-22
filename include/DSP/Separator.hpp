#pragma once

#include "DSP/AudioLoader.hpp"
#include "DSP/Cepstrum.hpp"

namespace eppyphany::DSP {
    class Separator {
        public:
            Separator(int fftSize, int hopSize);

            std::vector<Audio> Isolate(const Audio& audio, double minF0, double maxF0, double k = 2.0);

        private:
            std::vector<double> _calculatePowerSpectrum(const std::vector<std::complex<double>>& fft);
            double _calculateSFM(const std::vector<double>& powerSpectrum); // spectral flatness measure
            bool _isPeakSignificant(const std::vector<double>& cepstrum, int minLag, int maxLag, int peakLag) const;
            double _calculateSpectralEntropy(const std::vector<double>& powerSpectrum) const;
            bool _isResidualWhiteNoise(double sfm, int M) const;
            double _weight(double confidence, double k, double n = 2.0);

            int fftSize_;
            int hopSize_;
            Cepstrum ceps_;
    };
}