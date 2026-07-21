#pragma once

#include "DSP/AudioLoader.hpp"
#include "DSP/Cepstrum.hpp"

namespace eppyphany::DSP {
    class Separator {
        public:
            Separator(int fftSize, int hopSize);
            ~Separator();

            Audio Isolate(const Audio& audio, double minF0, double maxF0, double k);

        private:
            double _weight(double confidence, double k, double n = 2.0);

            int fftSize_;
            int hopSize_;
            Cepstrum ceps_;
    };
}