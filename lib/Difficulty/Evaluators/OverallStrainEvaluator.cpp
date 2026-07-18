#include "Difficulty/Evaluators/OverallStrainEvaluator.hpp"
#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"
#include <cmath>
#include <algorithm>

namespace eppyphany::Difficulty {
    double OverallStrainEvaluator::EvaluateDifficultyOf(const ManiaDifficultyHitObject& current) {
        double startTime = current.Start;
        double endTime = current.End;
        bool isOverlapping = false;

        double closestEndTime = std::abs(endTime - startTime); 
        double holdFactor = 1.0;
        double holdAddition = 0.0;

        for (const auto* previous : current.PreviousHitObjects) {
            if (previous == nullptr) {
                continue;
            }

            bool prevEndOverStartTime  = (previous->End - startTime > 1.0);
            bool currEndOverPrevEndTime = (endTime - previous->End > 1.0);
            bool currStartOverPrevStart = (startTime - previous->Start > 1.0);
            bool prevEndOverCurrEndTime = (previous->End - endTime > 1.0);

            isOverlapping |= (prevEndOverStartTime && currEndOverPrevEndTime && currStartOverPrevStart);

            if (prevEndOverCurrEndTime && currStartOverPrevStart) {
                holdFactor = 1.25;
            }

            closestEndTime = std::min(closestEndTime, std::abs(endTime - previous->End));
        }

        if (isOverlapping) {
            holdAddition = closestEndTime / (1 + std::exp(0.27 * 30 - 1));
        }

        return (1.0 + holdAddition) * holdFactor;
    }

}