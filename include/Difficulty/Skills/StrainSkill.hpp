#pragma once

#include "Skill.hpp"
#include <vector>

namespace eppyphany::Difficulty {
    class StrainSkill : public Skill {
        private:
            double _currentSectionPeak;
            double _currentSectionEnd;

            std::vector<double> _strainPeaks;

        public:
            const double DECAY_WEIGHT = 0.9;
            const int SECTION_LENGTH = 400;
            
            double DifficultyValue() override;

        protected: 
            double ProcessInternal(DifficultyHitObject& current) override;

            virtual double StrainValueAt(const DifficultyHitObject& current) = 0;
            virtual double CalculateInitialStrain(double time, const DifficultyHitObject& current) = 0;

            virtual std::vector<double> GetCurrentStrainPeaks();

        private:
            void _startNewSectionFrom(double time, const DifficultyHitObject& current);
            void _saveCurrentPeak();
    };
}