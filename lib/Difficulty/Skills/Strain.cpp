#include "Difficulty/Skills/Strain.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"
#include "Difficulty/Evaluators/IndividualStrainEvaluator.hpp"
#include "Difficulty/Evaluators/OverallStrainEvaluator.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace eppyphany::Difficulty {
    Strain::Strain(int columns) {
        individualStrains.resize(columns, 0.0);
    }

double Strain::StrainValueOf(const DifficultyHitObject& current) {
    auto& maniaCurrent = static_cast<const ManiaDifficultyHitObject&>(current);

    individualStrains[maniaCurrent.Column] = _applyDecay(individualStrains[maniaCurrent.Column], maniaCurrent.ColumnStrainTime, INDIVIDUAL_DECAY_BASE);
    individualStrains[maniaCurrent.Column] += indDiff;

    highestIndividualStrain = maniaCurrent.Delta <= 1 ? std::max(highestIndividualStrain, individualStrains[maniaCurrent.Column]) : individualStrains[maniaCurrent.Column];

    overallStrain = _applyDecay(overallStrain, maniaCurrent.Delta, OVERALL_DECAY_BASE);
    overallStrain += ovrDiff;

    return highestIndividualStrain + overallStrain - CurrentStrain;
}

    double Strain::CalculateInitialStrain(double time, const DifficultyHitObject& current) {
        auto* prev = current.Previous();
    
        if (!prev) {
            return 0.0;
        }

        return _applyDecay(highestIndividualStrain, time - prev->Start, INDIVIDUAL_DECAY_BASE) + 
               _applyDecay(overallStrain, time - prev->Start, OVERALL_DECAY_BASE);
    };

    double Strain::_applyDecay(double value, double delta, double decayBase) {
        return value * std::pow(decayBase, delta / 1000);
    };
}