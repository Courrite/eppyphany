#pragma once

#include "StrainDecaySkill.hpp"
#include <vector>

namespace eppyphany::Difficulty {
    class Strain : public StrainDecaySkill {
        public:
            const double INDIVIDUAL_DECAY_BASE = 0.125;
            const double OVERALL_DECAY_BASE = 0.3;

            Strain(int columns);

        protected:
            double StrainValueOf(const DifficultyHitObject &current) override;
            double CalculateInitialStrain(double time, const DifficultyHitObject &current) override;

        private:
            std::vector<double> individualStrains;
            double highestIndividualStrain = 0.0;
            double overallStrain = 1.0;

            double _applyDecay(double value, double delta, double decayBase);
    };
}