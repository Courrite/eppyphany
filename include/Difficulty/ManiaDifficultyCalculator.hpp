#pragma once

#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Difficulty/DifficultyCalculator.hpp"
#include "Difficulty/Skills/Skill.hpp"
#include "Generation/dotosu.hpp"
#include <vector>
#include <memory>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    class ManiaDifficultyCalculator : public DifficultyCalculator {
        public:
            double Calculate(const dotosu& osuFile) override;

        protected:
            void SortObjects(std::vector<std::unique_ptr<DifficultyHitObject>>& objects) override;
            std::vector<std::unique_ptr<DifficultyHitObject>> CreateDifficultyHitObjects(const dotosu& osuFile) override;
            std::vector<std::unique_ptr<Skill>> CreateSkills(const dotosu& osuFile) override;

        private:
            static constexpr double DIFFICULTY_MULTIPLIER = 0.018;
    };
}