#include "Difficulty/DifficultyCalculator.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include <algorithm>
#include <memory>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    double DifficultyCalculator::Calculate(const dotosu& osuFile) {
        auto objects = CreateDifficultyHitObjects(osuFile);
        
        SortObjects(objects);

        auto skills = CreateSkills(osuFile);

        for (auto& hitObject : objects) {
            for (auto& skill : skills) {
                if (skill && hitObject) {
                    skill->Process(*hitObject);
                }
            }
        }

        double totalDifficulty = 0;
        for (auto& skill : skills) {
            if (skill) {
                totalDifficulty += skill->DifficultyValue(); 
            }
        }

        return totalDifficulty;
    }

    void DifficultyCalculator::SortObjects(std::vector<std::unique_ptr<DifficultyHitObject>>& objects) {
        std::sort(objects.begin(), objects.end(), 
            [](const std::unique_ptr<DifficultyHitObject>& a, const std::unique_ptr<DifficultyHitObject>& b) -> bool {
                if (!a || !b) return false;
                return a->Start < b->Start;
            });
    }

    std::vector<std::unique_ptr<DifficultyHitObject>> DifficultyCalculator::CreateDifficultyHitObjects(const dotosu& osuFile) {
        std::vector<std::unique_ptr<DifficultyHitObject>> difficultyObjects;
        const auto& hitObjects = osuFile.GetHitObjects();

        difficultyObjects.reserve(hitObjects.size());

        for (size_t i = 0; i < hitObjects.size(); ++i) {
            const auto& current = hitObjects[i];
            const auto& last = (i > 0) ? hitObjects[i - 1] : current;
            
            difficultyObjects.push_back(
                std::make_unique<DifficultyHitObject>(current, last, &difficultyObjects, static_cast<int>(i))
            );
        }

        return difficultyObjects;
    }
}