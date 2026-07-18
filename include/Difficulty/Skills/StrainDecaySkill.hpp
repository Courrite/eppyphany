#pragma once

#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "StrainSkill.hpp"

using namespace eppyphany::Difficulty;

namespace eppyphany::Difficulty {
    class StrainDecaySkill : public StrainSkill {
        protected:
            double SkillMultiplier = 1.0;
            double StrainDecayBase = 1.0;
            double CurrentStrain = 0.0;

            virtual double StrainValueOf(const DifficultyHitObject& current) = 0;
            double StrainValueAt(const DifficultyHitObject& current) override;
            double CalculateInitialStrain(double time, const DifficultyHitObject& current) override;

        private:
            double _strainDecay(double ms);
    };
}