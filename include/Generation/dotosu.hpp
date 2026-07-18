#pragma once

#include "Objects.hpp"
#include <filesystem>
#include <fstream>
#include <vector>

namespace eppyphany::Generation {
    class dotosu {
    public:
        const double MillisecondsPerBeat;
        const dotosuFileConfig Config;

        dotosu(const dotosuFileConfig& _config);

        void AddHitObject(HitObject _hitObject);
        void AddHitObject(int columnIndex, int hitTime, int releaseTime = -1);
        void ClearHitObjects();
        void AddTimingPoint(TimingPoint tp);
        void ClearTimingPoints();
        const std::vector<HitObject>& GetHitObjects() const;
        bool Save(const std::filesystem::path& out);

    private:
        std::ifstream inputStream_;
        std::vector<HitObject> hitObjects_;
        std::vector<TimingPoint> timingPoints_;

        int _calculateLaneX(int columnIndex) const;
    };
}