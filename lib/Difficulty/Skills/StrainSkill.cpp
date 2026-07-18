#include "Difficulty/Skills/StrainSkill.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include <algorithm>
#include <libavutil/log.h>
#include <vector>
#include <cmath>

namespace eppyphany::Difficulty {
    double StrainSkill::DifficultyValue() {
        double difficulty = 0.0;
        double weight = 1.0;

        std::vector<double> peaks = GetCurrentStrainPeaks();
        std::vector<double> filtered;
        filtered.reserve(peaks.size());
        for (double p : peaks) {
            if (p > 0.0) filtered.push_back(p);
        }

        std::sort(filtered.begin(), filtered.end(), std::greater<double>());

        for (double strain : filtered) {
            difficulty += strain * weight;
            weight *= _decayWeight;
        }

        return difficulty;
    }

    double StrainSkill::ProcessInternal(DifficultyHitObject& current) {
        if (current.Index == 0)
            _currentSectionEnd = std::ceil(current.Start / _sectionLength) * _sectionLength;

        while (current.Start > _currentSectionEnd) {
            _saveCurrentPeak();
            _startNewSectionFrom(_currentSectionEnd, current);
            _currentSectionEnd += _sectionLength;
        }

        double strain = StrainValueAt(current);
        _currentSectionPeak = std::max(strain, _currentSectionPeak);

        return strain;
    }

    std::vector<double> StrainSkill::GetCurrentStrainPeaks() {
        std::vector<double> peaks = _strainPeaks;
    
        peaks.push_back(_currentSectionPeak);

        return peaks;
    }

    void StrainSkill::_saveCurrentPeak() {
        _strainPeaks.push_back(_currentSectionPeak);
    }

    void StrainSkill::_startNewSectionFrom(double time, const DifficultyHitObject& current) {
        _currentSectionPeak = CalculateInitialStrain(time, current);
    }
}