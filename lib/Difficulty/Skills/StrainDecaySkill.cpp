#include "Difficulty/Skills/StrainDecaySkill.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include <cmath>

namespace eppyphany::Difficulty {
    double StrainDecaySkill::CalculateInitialStrain(double time, const DifficultyHitObject& current) {
        return CurrentStrain * _strainDecay(time - current.Previous()->Start);
    }

    double StrainDecaySkill::StrainValueAt(const DifficultyHitObject& current) {
        CurrentStrain *= _strainDecay(current.Delta);
        CurrentStrain += StrainValueOf(current) * SkillMultiplier;

        return CurrentStrain;
    }

    double StrainDecaySkill::_strainDecay(double ms){ 
        return std::pow(StrainDecayBase, ms / 1000); 
    }
}