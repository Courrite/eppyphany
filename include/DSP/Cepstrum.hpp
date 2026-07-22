#pragma once

#include <vector>

extern "C" {
    #include <kissfft/kiss_fft.h>
}

namespace eppyphany::DSP {
    struct F0Estimate {
        double Frequency;
        double Confidence;
    };

    class Cepstrum {
        public:
            Cepstrum(int n);
            ~Cepstrum();

            std::vector<std::complex<double>> ComputeFFT(const std::vector<double>& frame);
            void Forward(const std::vector<double>& timeIn, std::vector<double>& cepstrumOut);
            void Inverse(const std::vector<double>& cepstrumIn, std::vector<double>& timeOut);
            void ApplyMask(std::vector<double>& cepstrum, const std::vector<double>& mask);
            F0Estimate EstimateF0(const std::vector<double>& cepstrum, double sampleRate, double minF0 = 50.0, double maxF0 = 2000.0);
            void BuildCombMask(double f0Hz, double sampleRate, std::vector<double>& maskOut, int numHarmonics = 20, int bandwidth = 1);
            double PercussiveEnergy(const std::vector<double>& cepstrum, int loBin, int hiBin);
            void ApplyHannWindow(std::vector<double>& samples);

        private:
            int n_;
            kiss_fft_cfg cfgForward_;
            kiss_fft_cfg cfgInverse_;
            kiss_fft_cfg fftPlan_;
            std::vector<std::complex<double>> fftIn_, fftOut_, cepIn_, cepTemp_;
            std::vector<double> workspaceReal_;
            std::vector<double> phase_;
    };
}