#pragma once

#include <kissfft/kiss_fftr.h>
#include <vector>
#include <filesystem>
#include <complex>

namespace eppyphany::Generation {

    struct AudioFrame {
        double TimestampMs;
        float SubBassEnergy; // 20-60 hz (kick or drums)
        float MidEnergy; // 250-2000 hz (vocals or instruments)
        float HighEnergy; // 4000-20000 hz (cymbals or snares)
        float TotalFlux; // spectral change score
    };

    class DSProcessor {
    public:
        DSProcessor(size_t fftSize = 1024, size_t hopSize = 512);
        ~DSProcessor();

        bool LoadAudio(const std::filesystem::path& audioPath);
        std::vector<AudioFrame> Analyze();

    private:
        size_t fftSize_;
        kiss_fftr_cfg fftPlan_;
        size_t hopSize_;
        unsigned int sampleRate_ = 44100;
        
        std::vector<float> monoAudioData_;

        void _applyHannWindow(std::vector<float>& frame);
        std::vector<std::complex<float>> _computeFFT(const std::vector<float>& frame);
        float _calculateSpectralFlux(const std::vector<float>& currentMag, const std::vector<float>& prevMag);
    };

}