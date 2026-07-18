#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"

namespace eppyphany::Difficulty {
    DifficultyHitObject::DifficultyHitObject(
        const eppyphany::Generation::HitObject& baseObject, 
        const eppyphany::Generation::HitObject& lastObject, 
        const std::vector<std::unique_ptr<DifficultyHitObject>>* hitObjects, 
        int idx
    ) : BaseObject(baseObject), 
        LastObject(lastObject), 
        difficultyHitObjects_(hitObjects), 
        Index(idx) 
    {
        Start = baseObject.HitTime;
        End = (baseObject.ReleaseTime != -1) ? baseObject.ReleaseTime : baseObject.HitTime;
        Delta = Start - lastObject.HitTime;
    }

    const DifficultyHitObject* DifficultyHitObject::Previous() const {
        int prevIdx = Index - 1;
        if (prevIdx >= 0 && prevIdx < static_cast<int>(difficultyHitObjects_->size())) {
            return (*difficultyHitObjects_)[prevIdx].get();
        }
        return nullptr;
    }

    const DifficultyHitObject* DifficultyHitObject::Next() const {
        int nextIdx = Index + 1;
        if (nextIdx >= 0 && nextIdx < static_cast<int>(difficultyHitObjects_->size())) {
            return (*difficultyHitObjects_)[nextIdx].get();
        }
        return nullptr;
    }
}