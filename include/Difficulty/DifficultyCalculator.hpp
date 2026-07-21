#pragma once

#include <vector>
#include <memory>
#include "Generation/dotosu.hpp"
#include "Skills/Skill.hpp"
#include "Preprocessing/DifficultyHitObject.hpp"

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    class DifficultyCalculator {
        public:
            virtual ~DifficultyCalculator() {};

            virtual double Calculate(const dotosu& osuFile);

        protected:
            virtual std::vector<std::unique_ptr<Skill>> CreateSkills(const dotosu& osuFile) = 0;
            virtual std::vector<std::unique_ptr<DifficultyHitObject>> CreateDifficultyHitObjects(const dotosu& osuFile);
    };
}