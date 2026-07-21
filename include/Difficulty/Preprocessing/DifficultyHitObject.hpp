#pragma once

#include "Generation/dotosu.hpp"

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    class DifficultyHitObject {
        public:
            virtual ~DifficultyHitObject() = default;

            int Index;
            HitObject BaseObject;
            const DifficultyHitObject* LastObject;
            double Delta;
            double Start;
            double End;

        public:
            DifficultyHitObject(const HitObject& baseObject, const DifficultyHitObject* lastObject, int idx);

            const DifficultyHitObject* Previous() const;
    };
}