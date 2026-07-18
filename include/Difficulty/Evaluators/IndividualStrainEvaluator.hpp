#pragma once

#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"

namespace eppyphany::Difficulty {
    class IndividualStrainEvaluator {
        public:
            static double EvaluateDifficultyOf(const ManiaDifficultyHitObject& current);
    };
}