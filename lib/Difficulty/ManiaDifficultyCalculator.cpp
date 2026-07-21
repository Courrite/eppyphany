#include "Difficulty/ManiaDifficultyCalculator.hpp"
#include "Difficulty/Preprocessing/DifficultyHitObject.hpp"
#include "Difficulty/Preprocessing/ManiaDifficultyHitObject.hpp"
#include "Difficulty/Skills/Strain.hpp"
#include <algorithm>
#include <memory> 

using namespace eppyphany::Generation;

namespace eppyphany::Difficulty {
    double ManiaDifficultyCalculator::Calculate(const dotosu& osuFile) {
        auto objects = CreateDifficultyHitObjects(osuFile);

        auto skills = CreateSkills(osuFile);

        for (const auto& hitObject : objects) {
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

        return totalDifficulty * DIFFICULTY_MULTIPLIER;
    }

    std::vector<std::unique_ptr<DifficultyHitObject>> ManiaDifficultyCalculator::CreateDifficultyHitObjects(const dotosu& osuFile) {
        auto hitObjects = osuFile.GetHitObjects();
        if (hitObjects.empty()) return {};

        std::sort(hitObjects.begin(), hitObjects.end(), [](const auto& a, const auto& b) {
            return a.HitTime < b.HitTime;
        });

        std::vector<std::unique_ptr<DifficultyHitObject>> objects;
        objects.reserve(hitObjects.size());

        int totalColumns = osuFile.Config.Keys;
        std::vector<ManiaDifficultyHitObject*> lastObjectInColumn(totalColumns, nullptr);
        std::unique_ptr<ManiaDifficultyHitObject> lastDiffObj = nullptr;
    
        ManiaDifficultyHitObject* prevOverall = nullptr;

        for (int i = 0; i < hitObjects.size(); ++i) {
            const auto& current = hitObjects[i];

            int column = current.Column - 1;

            ManiaDifficultyHitObject* prevInColumn = lastObjectInColumn[column];

            auto maniaObj = std::make_unique<ManiaDifficultyHitObject>(
                current, 
                prevInColumn, 
                prevOverall,
                objects.size(),
                totalColumns
            );

            prevOverall = maniaObj.get(); 
            lastObjectInColumn[column] = maniaObj.get();
            objects.push_back(std::move(maniaObj));
        }

        return objects;
    }

    std::vector<std::unique_ptr<Skill>> ManiaDifficultyCalculator::CreateSkills(const dotosu& osuFile) {
        std::vector<std::unique_ptr<Skill>> skills;
        int totalColumns = osuFile.Config.Keys;
        
        skills.push_back(std::make_unique<Strain>(totalColumns));
        
        return skills;
    }
}