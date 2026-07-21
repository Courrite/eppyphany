#include "Generation/dotosu.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace eppyphany::Generation {
    dotosu::dotosu(const MapConfig& _config) 
        : Config(_config),
          MillisecondsPerBeat(60000.0 / (_config.BPM > 0 ? _config.BPM : 120)),
          inputStream_(std::filesystem::current_path() / (_config.Name + ".osu"), std::ios::in)
    {
    }

    int dotosu:: _calculateLaneX(int columnIndex) const {
        if (Config.Keys == 0) return 256;
        return columnIndex * (512 / Config.Keys) + (256 / Config.Keys);
    }

    void dotosu::AddTimingPoint(TimingPoint tp) {
        timingPoints_.push_back(tp);
    }

    void dotosu::ClearTimingPoints() {
        timingPoints_.clear();
    }

    void dotosu::AddHitObject(HitObject _hitObject) {
        hitObjects_.push_back(_hitObject);
    }

    void dotosu::AddHitObject(int columnIndex, double hitTime, double releaseTime) {
        HitObject obj;
        obj.Column = columnIndex;
        obj.HitTime = hitTime;
        obj.ReleaseTime = releaseTime;
        obj.Type = (releaseTime > hitTime) ? HitObjectType::LongNote : HitObjectType::Note;
        hitObjects_.push_back(obj);
    }

    void dotosu::ClearHitObjects() {
        hitObjects_.clear();
    }

    const std::vector<HitObject>& dotosu::GetHitObjects() const {
        return hitObjects_;
    }

    bool dotosu::Save(const std::filesystem::path& baseOutputDirectory) {
        std::string folderName = Config.Author + " - " + Config.Name;
        std::filesystem::path mapDirectory = baseOutputDirectory / folderName;

        try {
            if (!std::filesystem::exists(mapDirectory)) {
                std::filesystem::create_directories(mapDirectory);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to create directory tree: " << e.what() << std::endl;
            return false;
        }

        if (!Config.AudioFile.empty() && std::filesystem::exists(Config.AudioFile)) {
            std::filesystem::path destinationAudio = mapDirectory / Config.AudioFile.filename();
            try {
                std::filesystem::copy_file(Config.AudioFile, destinationAudio, std::filesystem::copy_options::overwrite_existing);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Warning: Could not copy audio file: " << e.what() << std::endl;
            }
        } else {
            std::cerr << "Warning: Source audio file not found at " << Config.AudioFile << std::endl;
        }

        std::string filename = Config.Author + " - " + Config.Name + " (" + Config.Creator + ") [" + Config.DifficultyName + "].osu";
        std::filesystem::path fullPath = mapDirectory / filename;

        std::ofstream outFile(fullPath, std::ios::out | std::ios::trunc);
        if (!outFile.is_open()) {
            std::cerr << "Failed to open file for writing: " << fullPath << std::endl;
            return false;
        }

        outFile << "osu file format v128\n\n";

        outFile << "[General]\n";
        outFile << "AudioFilename: " << Config.AudioFile.filename().string() << "\n";
        outFile << "AudioLeadIn: 0\n";
        outFile << "PreviewTime: " << Config.PreviewTime << "\n";
        outFile << "Countdown: 0\n";
        outFile << "SampleSet: Normal\n";
        outFile << "StackLeniency: 0.7\n";
        outFile << "Mode: 3\n"; 
        outFile << "LetterboxInBreaks: 0\n";
        outFile << "SpecialStyle: 0\n";
        outFile << "UseSkinSprites: 0\n";
        outFile << "WidescreenStoryboard: 1\n\n";

        outFile << "[Editor]\n";
        outFile << "DistanceSpacing: 1\n";
        outFile << "BeatDivisor: 8\n";
        outFile << "GridSize: 0\n";
        outFile << "TimelineZoom: 1.0\n";
        outFile << "VelocityPresets: 0.75,1,1.5\n\n";

        outFile << "[Metadata]\n";
        outFile << "Title:" << Config.Name << "\n";
        outFile << "TitleUnicode:" << Config.Name << "\n";
        outFile << "Artist:" << Config.Author << "\n";
        outFile << "ArtistUnicode:" << Config.Author << "\n";
        outFile << "Creator:" << Config.Creator << "\n";
        outFile << "Version:" << Config.DifficultyName << "\n";
        outFile << "Source:" << Config.Source << "\n";
        outFile << "Tags: hypertrance eppyphany\n";
        outFile << "BeatmapID:0\n";
        outFile << "BeatmapSetID:-1\n\n";

        outFile << "[Difficulty]\n";
        outFile << "HPDrainRate:" << Config.HPDrainRate << "\n";
        outFile << "CircleSize:" << Config.Keys << "\n"; 
        outFile << "OverallDifficulty:" << Config.OverallDifficulty << "\n";
        outFile << "ApproachRate:5\n";
        outFile << "SliderMultiplier:1.3999999687075613\n";
        outFile << "SliderTickRate:1\n\n";

        outFile << "[Events]\n";
        outFile << "// Background and Video events\n";
        outFile << "// Storyboard Layer 0 (Background)\n";
        outFile << "// Storyboard Layer 1 (Fail)\n";
        outFile << "// Storyboard Layer 2 (Pass)\n";
        outFile << "// Storyboard Layer 3 (Foreground)\n";
        outFile << "// Storyboard Layer 4 (Overlay)\n";
        outFile << "// Storyboard Sound Samples\n";
        outFile << "// Break Periods\n\n";

        outFile << "[TimingPoints]\n";
        for (const auto& tp : timingPoints_) {
            outFile << tp.OffsetMs << "," << tp.BeatLength << "," << tp.Meter << "," 
            << tp.SampleSet << "," << tp.SampleIndex << "," << tp.Volume << "," 
            << tp.Uninherited << "," << tp.Effects << "\n";
        }

        outFile << "[HitObjects]\n";
        for (const auto& obj : hitObjects_) {
            if (obj.Type == HitObjectType::LongNote) {
                outFile << _calculateLaneX(obj.Column) << ",192," << obj.HitTime << "," << obj.Type 
                        << ",0," << obj.ReleaseTime << ":1:0:0:100:\n";
            } else {
                outFile << _calculateLaneX(obj.Column) << ",192," << obj.HitTime << "," << obj.Type 
                        << ",0,1:0:0:100:\n";
            }
        }

        outFile.close();
        return true;
    }
}