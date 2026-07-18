#pragma once

#include "Skill.hpp"
#include <vector>

namespace eppyphany::Difficulty {
    class StrainSkill : public Skill {
        private:
            const double _decayWeight = 0.9;
            const int _sectionLength = 400;

            double _currentSectionPeak;
            double _currentSectionEnd;

            std::vector<double> _strainPeaks;

        public:
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