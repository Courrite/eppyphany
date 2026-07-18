#pragma once

#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Generation/Objects.hpp"
#include <vector>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    class ManiaDifficultyHitObject : public DifficultyHitObject {
        public:
            ManiaDifficultyHitObject();
            
            int Column;
            double ColumnStrainTime;
            double ColumnDelta;

            ManiaDifficultyHitObject* ImmediatePrevInColumn = nullptr;
            ManiaDifficultyHitObject* ImmediateNextInColumn = nullptr;
        
            std::vector<const ManiaDifficultyHitObject*> PreviousHitObjects;

            ManiaDifficultyHitObject(
                const HitObject& hitObject,
                ManiaDifficultyHitObject* prevInColumn,
                ManiaDifficultyHitObject* prevOverall,
                int index,
                int columnCount
            );

            ManiaDifficultyHitObject* PrevInColumn(int backwardsIndex);
            ManiaDifficultyHitObject* NextInColumn(int forwardsIndex);
    };
}