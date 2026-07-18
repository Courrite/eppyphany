#include "Difficulty/Evaluators/IndividualStrainEvaluator.hpp"
#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"

namespace eppyphany::Difficulty {
    double IndividualStrainEvaluator::EvaluateDifficultyOf(const ManiaDifficultyHitObject& current) {
        double startTime = current.Start;
        double endTime = current.End;

        double holdFactor = 1.0;

        for (const auto* previous : current.PreviousHitObjects) {
            if (previous == nullptr) {
                continue;
            }

            if ((previous->End - endTime > 1.0) && 
                (startTime - previous->Start > 1.0)) 
            {
                holdFactor = 1.25;
                break;
            }
        }

        return 2.0 * holdFactor;
    }

}