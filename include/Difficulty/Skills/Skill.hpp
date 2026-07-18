#pragma once

#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include <vector>

namespace eppyphany::Difficulty {
    class Skill {
        public:
            virtual ~Skill();

            std::vector<double> ObjectDifficulties;

            void Process(DifficultyHitObject& current);

            virtual double DifficultyValue() = 0;

        protected:
            virtual double ProcessInternal(DifficultyHitObject& current) = 0;
    };
}