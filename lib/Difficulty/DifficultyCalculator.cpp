#include "Difficulty/DifficultyCalculator.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include <algorithm>
#include <memory>

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    double DifficultyCalculator::Calculate(const dotosu& osuFile) {
        auto objects = CreateDifficultyHitObjects(osuFile);

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

    std::vector<std::unique_ptr<DifficultyHitObject>> DifficultyCalculator::CreateDifficultyHitObjects(const dotosu& osuFile) {
        auto hitObjects = osuFile.GetHitObjects();

        std::sort(hitObjects.begin(), hitObjects.end(), [](const auto& a, const auto& b) {
            return a.HitTime < b.HitTime;
        });

        std::vector<std::unique_ptr<DifficultyHitObject>> objects;
        objects.reserve(hitObjects.size());

        DifficultyHitObject* last = nullptr; 

        for (int i = 0; i < hitObjects.size(); ++i) {
            auto newObj = std::make_unique<DifficultyHitObject>(
                hitObjects[i], 
                last,
                i
            );

            last = newObj.get(); 
            objects.push_back(std::move(newObj));
        }

        return objects;
    }
}