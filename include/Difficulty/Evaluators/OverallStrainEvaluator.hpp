#pragma once

#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"

namespace eppyphany::Difficulty {
    class OverallStrainEvaluator {
        public:
            static double EvaluateDifficultyOf(const ManiaDifficultyHitObject& current);
    };
}