#pragma once

#include "DSP/AudioLoader.hpp"
#include <vector>

extern "C" {
    #include <kissfft/kiss_fftr.h>
}

namespace eppyphany::DSP {
    struct Block {
        std::vector<double> Sum;
        int FirstFrame = 0, LastFrame = 0;
    };

    struct AudioFrame {
        double TimestampMs;
        double Brightness;
        double Rolloff;
        double TotalEnergy;
        double TotalFlux; // spectral change score
        double ChromaFlux; // tonal harmony and pitch change score
        double AttackSharpness = 0.0; // how percussive this frame's transient is
        double SectionNovelty = 0.0; // how different this ~1s block sounds vs recent history
        double PercussiveCepstralEnergy;
    };

    class Analyzer {
        public:
            Analyzer(int fftSize, int hopSize);

            std::vector<AudioFrame> Analyze(const Audio& audio);
            
        private:
            int fftSize_;
            double hopSize_;

            std::vector<double> _magnitude(const std::vector<std::complex<double>>& fft);
            std::vector<double> _computeChroma(const std::vector<double>& mag, double sampleRate);
            double _calculateSpectralFlux(const std::vector<double>& cur, const std::vector<double>& prev);
            void _computeAttackSharpness(std::vector<AudioFrame>& timeline, const std::vector<std::vector<double>>& spectra);
            void _computeSectionNovelty(std::vector<AudioFrame>& timeline, const std::vector<std::vector<double>>& spectra);
            void _computeSpectralShape(std::vector<AudioFrame>& timeline, const std::vector<std::vector<double>>& spectra, double sampleRate);
    };
}