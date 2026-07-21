#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    DifficultyHitObject::DifficultyHitObject(
        const HitObject& baseObject, 
        const DifficultyHitObject* lastObject, 
        int idx
    ) : BaseObject(baseObject), 
        LastObject(lastObject),
        Index(idx) 
    {
        Start = baseObject.HitTime;
        End = baseObject.ReleaseTime > baseObject.HitTime ? baseObject.ReleaseTime : baseObject.HitTime;
        Delta = lastObject != nullptr ? Start - lastObject->Start : 0;
    }

    const DifficultyHitObject* DifficultyHitObject::Previous() const {
        return LastObject;
    }
}