#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Generation/Objects.hpp"
#include <vector>
#include <memory>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
        ManiaDifficultyHitObject::ManiaDifficultyHitObject(
        const eppyphany::Generation::HitObject& hitObject,
        const eppyphany::Generation::HitObject& lastObject,
        const std::vector<std::unique_ptr<DifficultyHitObject>>* objects,
        ManiaDifficultyHitObject* prevInColumn,
        int index,
        int columnCount
    ) : DifficultyHitObject(hitObject, lastObject, objects, index) 
    {
        this->Column = hitObject.Column; 

        this->PreviousHitObjects.resize(columnCount, nullptr);
        
        this->ImmediatePrevInColumn = prevInColumn;
        if (prevInColumn) {
            prevInColumn->ImmediateNextInColumn = this;
        }

        this->ColumnStrainTime = prevInColumn ? (this->Start - prevInColumn->Start) : this->Start;

        if (index > 0) {
            auto* prevNote = static_cast<ManiaDifficultyHitObject*>((*objects)[index - 1].get());
            
            this->PreviousHitObjects.resize(prevNote->PreviousHitObjects.size(), nullptr);
            for (size_t i = 0; i < prevNote->PreviousHitObjects.size(); ++i) {
                this->PreviousHitObjects[i] = prevNote->PreviousHitObjects[i];
            }
            this->PreviousHitObjects[prevNote->Column] = prevNote;
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