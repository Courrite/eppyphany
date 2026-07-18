#pragma once

#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Generation/Objects.hpp"
#include <vector>
#include <memory>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    class ManiaDifficultyHitObject : public DifficultyHitObject {
        public:
            ManiaDifficultyHitObject();
            
            int Column;
            double ColumnStrainTime;

            ManiaDifficultyHitObject* ImmediatePrevInColumn = nullptr;
            ManiaDifficultyHitObject* ImmediateNextInColumn = nullptr;
        
            std::vector<ManiaDifficultyHitObject*> PreviousHitObjects;

            ManiaDifficultyHitObject(
                const HitObject& hitObject,
                const HitObject& lastObject,
                const std::vector<std::unique_ptr<DifficultyHitObject>>* objects,
                ManiaDifficultyHitObject* prevInCOlumn,
                int index,
                int columnCount
            );

            ManiaDifficultyHitObject* PrevInColumn(int backwardsIndex);
            ManiaDifficultyHitObject* NextInColumn(int forwardsIndex);
    };
}