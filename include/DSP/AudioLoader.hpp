#pragma once

#include <filesystem>
#include <vector>

namespace eppyphany::DSP {
    struct Audio {
        std::vector<double> Samples;
        double SampleRate;
    };

    class AudioLoader {
        public:
            static Audio LoadAudio(const std::filesystem::path& audioPath);
    };
}