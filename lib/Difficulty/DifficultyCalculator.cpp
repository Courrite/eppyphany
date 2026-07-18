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
        auto hitObjects = osuFile.GetHitObjects();
        std::vector<std::unique_ptr<DifficultyHitObject>> objects;
        objects.reserve(hitObjects.size());

        DifficultyHitObject* last = nullptr; 

        for (size_t i = 0; i < hitObjects.size(); ++i) {
            auto newObj = std::make_unique<DifficultyHitObject>(
                hitObjects[i], 
                last,
                static_cast<int>(i)
            );

            last = newObj.get(); 
        
            objects.push_back(std::move(newObj));
        }

        return objects;
    }
}