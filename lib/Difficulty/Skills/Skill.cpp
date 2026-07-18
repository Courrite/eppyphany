#include "Difficulty/Skills/Skill.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"

namespace eppyphany::Difficulty {
    Skill::~Skill() = default;

    void Skill::Process(DifficultyHitObject& current) {
        double difficultyValue = ProcessInternal(current);
        ObjectDifficulties.push_back(difficultyValue);
    }
}