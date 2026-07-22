#include "DSP/Cepstrum.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <complex>
#include <iostream>

extern "C" {
    #include <kissfft/kiss_fft.h>
}

namespace eppyphany::DSP {
    Cepstrum::Cepstrum(int n) : n_(n) {
        cfgForward_ = kiss_fft_alloc(n, 0, nullptr, nullptr);
        cfgInverse_ = kiss_fft_alloc(n, 1, nullptr, nullptr);

        fftPlan_ = kiss_fft_alloc(n_, /*inverse_fft=*/0, nullptr, nullptr);
        if (!fftPlan_) {
            std::cerr << "kiss_fftr_alloc failed for fftSize=" << n_ << "\n";
        }
        
        fftIn_.resize(n);
        fftOut_.resize(n);
        cepIn_.resize(n);
        cepTemp_.resize(n);
        phase_.resize(n / 2 + 1);
    }

    Cepstrum::~Cepstrum() {
        kiss_fft_free(cfgForward_);
        kiss_fft_free(cfgInverse_);
        kiss_fft_free(fftPlan_);
    }

    std::vector<std::complex<double>> Cepstrum::ComputeFFT(const std::vector<double>& frame) {
        int binCount = n_ / 2 + 1;

        if (!fftPlan_ || frame.size() != n_) {
            return std::vector<std::complex<double>>(binCount, 0.0);
        }

        for (int i = 0; i < n_; ++i) {
            fftIn_[i] = {frame[i], 0.0};
        }

        kiss_fft(
            fftPlan_, 
            reinterpret_cast<const kiss_fft_cpx*>(fftIn_.data()), 
            reinterpret_cast<kiss_fft_cpx*>(fftOut_.data())
        );

        std::vector<std::complex<double>> output(binCount);
        for (int i = 0; i < binCount; ++i) {
            output[i] = fftOut_[i];
        }

        return output;
    }

    void Cepstrum::Forward(const std::vector<double>& timeIn, std::vector<double>& CepstrumOut) {
        for (int i = 0; i < n_; ++i) {
            fftIn_[i] = {timeIn[i], 0.0};
        }
        
        kiss_fft(
            cfgForward_, 
            reinterpret_cast<const kiss_fft_cpx*>(fftIn_.data()), 
            reinterpret_cast<kiss_fft_cpx*>(fftOut_.data())
        );
        
        std::fill(cepIn_.begin(), cepIn_.end(), std::complex<double>{0.0, 0.0});
        
        for (int i = 0; i < n_ / 2 + 1; ++i) {
            double mag = std::abs(fftOut_[i]);

            phase_[i] = std::arg(fftOut_[i]);
            cepIn_[i].real(std::log1p(mag));
        }
        
        for (int i = 1; i < n_ / 2; ++i) {
            cepIn_[n_ - i].real(cepIn_[i].real());
        }
        
        kiss_fft(
            cfgInverse_, 
            reinterpret_cast<const kiss_fft_cpx*>(cepIn_.data()), 
            reinterpret_cast<kiss_fft_cpx*>(cepTemp_.data())
        );
        
        double invN = 1.0 / static_cast<double>(n_);
        for (int i = 0; i < n_; ++i) {
            CepstrumOut[i] = cepTemp_[i].real() * invN;
        }
    }

    void Cepstrum::Inverse(const std::vector<double>& CepstrumIn, std::vector<double>& timeOut) {
        for (int i = 0; i < n_; ++i) {
            cepIn_[i] = {CepstrumIn[i], 0.0};
        }
        
        kiss_fft(
            cfgForward_, 
            reinterpret_cast<const kiss_fft_cpx*>(cepIn_.data()), 
            reinterpret_cast<kiss_fft_cpx*>(fftOut_.data())
        );
        
        std::fill(cepIn_.begin(), cepIn_.end(), std::complex<double>{0.0, 0.0});
        for (int i = 1; i < n_ / 2 + 1; ++i) {
            double mag = std::exp(fftOut_[i].real());
            cepIn_[i] = std::polar(mag, phase_[i]);
        }
        
        for (int i = 1; i < (n_ + 1) / 2; ++i) {
            cepIn_[n_ - i] = std::conj(cepIn_[i]);
        }
        
        kiss_fft(
            cfgInverse_, 
            reinterpret_cast<const kiss_fft_cpx*>(cepIn_.data()), 
            reinterpret_cast<kiss_fft_cpx*>(cepTemp_.data())
        );
        
        double invN = 1.0 / static_cast<double>(n_);
        for (int i = 0; i < n_; ++i) {
            timeOut[i] = cepTemp_[i].real() * invN;
        }
    }

    void Cepstrum::ApplyMask(std::vector<double>& Cepstrum, const std::vector<double>& mask) {
        for (int i = 0; i < n_; ++i) {
            Cepstrum[i] *= mask[i];
        }
    }

    F0Estimate Cepstrum::EstimateF0(const std::vector<double>& Cepstrum, double sampleRate, double minF0, double maxF0) {
        int loQ = std::max<int>(2, static_cast<int>(sampleRate / maxF0));
        int hiQ = std::min<int>(n_ / 2 - 1, static_cast<int>(sampleRate / minF0));
        if (loQ >= hiQ) return {0.0, 0.0};

        int peakIdx = loQ;
        double peakVal = Cepstrum[loQ];
        for (int i = loQ + 1; i <= hiQ; ++i) {
            if (Cepstrum[i] > peakVal) { peakVal = Cepstrum[i]; peakIdx = i; }
        }

        double sum = 0.0;
        int count = 0;
        for (int i = loQ; i <= hiQ; ++i) {
            if (i + 2 >= peakIdx && i <= peakIdx + 2) continue;
            sum += std::abs(Cepstrum[i]);
            ++count;
        }

        double baseline = count > 0 ? sum / count : 0.0;
        double confidence = baseline > 1e-6 ? peakVal / baseline : 0.0;

        return {sampleRate / static_cast<double>(peakIdx), confidence};
    }

    void Cepstrum::BuildCombMask(double f0Hz, double sampleRate, std::vector<double>& maskOut, int numHarmonics, int bandwidth) {
        maskOut.assign(n_, 0.0);
        if (f0Hz <= 0.0)
            return;

        double period = sampleRate / f0Hz;
        for (int h = 1; h <= numHarmonics; ++h) {
            int center = std::lround(period * h);
            if (center >= n_) break;

            for (int off = -bandwidth; off <= bandwidth; ++off) {
                int idx = center + off;
                if (idx <= 0 || idx >= n_) continue;
                maskOut[idx] = 1.0;
                maskOut[n_ - idx] = 1.0;
            }
        }
    }

    double Cepstrum::PercussiveEnergy(const std::vector<double>& Cepstrum, int loBin, int hiBin) {
        double percussiveEnergy = 0.0;
        for (int i = loBin; i < hiBin; ++i)
            percussiveEnergy += std::abs(Cepstrum[i]);

        return percussiveEnergy;
    }

    void Cepstrum::ApplyHannWindow(std::vector<double>& samples) {
        if (samples.size() < 2) return;
        const double invN = 1.0 / (samples.size() - 1.0);
        for (int i = 0; i < samples.size(); ++i) {
            double v = 0.5 * (1.0 - std::cos(2.0 * M_PI * i * invN));
            samples[i] *= v;
        }
    }
}