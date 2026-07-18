#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Generation/Objects.hpp"
#include <vector>
#include <memory>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
        ManiaDifficultyHitObject::ManiaDifficultyHitObject(
        const HitObject& hitObject,
        ManiaDifficultyHitObject* prevInColumn,
        ManiaDifficultyHitObject* prevOverall,
        int index,
        int columnCount
    ) : DifficultyHitObject(hitObject, prevOverall, index) 
    {
        this->Column = hitObject.Column; 

        this->ColumnDelta = prevOverall != nullptr ? hitObject.HitTime - prevOverall->Start : 0;

        this->PreviousHitObjects.resize(columnCount, nullptr);
        
        this->ImmediatePrevInColumn = prevInColumn;
        if (prevInColumn) {
            prevInColumn->ImmediateNextInColumn = this;
        }

        this->ColumnStrainTime = prevInColumn ? (this->Start - prevInColumn->Start) : this->Start;

        if (index > 0) {
            if (prevOverall) {
                this->PreviousHitObjects = prevOverall->PreviousHitObjects;
                this->PreviousHitObjects[prevOverall->Column] = prevOverall;
            }
        }
    }

    ManiaDifficultyHitObject* ManiaDifficultyHitObject::PrevInColumn(int backwardsIndex) {
        ManiaDifficultyHitObject* curr = this->ImmediatePrevInColumn;
        for (int i = 0; i < backwardsIndex && curr != nullptr; ++i) {
            curr = curr->ImmediatePrevInColumn;
        }
        return curr;
    }

    ManiaDifficultyHitObject* ManiaDifficultyHitObject::NextInColumn(int forwardsIndex) {
        ManiaDifficultyHitObject* curr = this->ImmediateNextInColumn;
        for (int i = 0; i < forwardsIndex && curr != nullptr; ++i) {
            curr = curr->ImmediateNextInColumn;
        }
        return curr;
    }
}