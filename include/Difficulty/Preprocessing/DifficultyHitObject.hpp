#pragma once

#include "Generation/Objects.hpp"
#include <vector>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    class DifficultyHitObject {
        public:
            int Index;
            HitObject BaseObject;
            HitObject LastObject;
            double Delta;
            double Start;
            double End;

        private:
            const std::vector<std::unique_ptr<DifficultyHitObject>>* difficultyHitObjects_;

        public:
            DifficultyHitObject(const HitObject& baseObject, const HitObject& lastObject, const std::vector<std::unique_ptr<DifficultyHitObject>>* hitObjects, int idx);

            const DifficultyHitObject* Previous() const;
            const DifficultyHitObject* Next() const;
    };
}