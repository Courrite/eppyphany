#include "DSP/Analyzer.hpp"
#include "DSP/AudioLoader.hpp"
#include "Difficulty/ManiaDifficultyCalculator.hpp"
#include "Generation/dotosu.hpp"
#include "Generation/Composer.hpp"
#include <filesystem>
#include <iostream>
#include <string>

using namespace eppyphany::Generation;
using namespace eppyphany::Difficulty;
using namespace eppyphany::DSP;

int main() {
    std::string name;
    std::string author;
    std::string creator;
    std::string source;
    std::string difficultyName;
    std::filesystem::path audioFile;
    unsigned int keys = 4;
    unsigned int bpm = 120;

    double hpDrain = 5.0;
    double accuracy = 5.0;
    double proposedSr = 0.0;

    std::string temp;

    // user prompts
    std::cout << "Name of the map: ";
    std::getline(std::cin, name);

    std::cout << "Creator of the map: ";
    std::getline(std::cin, creator);

    std::cout << "Author of the song: ";
    std::getline(std::cin, author);

    std::cout << "Source of the song: ";
    std::getline(std::cin, source);

    std::cout << "Audio File: ";
    std::getline(std::cin, temp);
    audioFile = temp;

    std::cout << "Keys: ";
    std::getline(std::cin, temp);
    if (!temp.empty()) keys = std::stoul(temp);

    std::cout << "Beats Per Minute: ";
    std::getline(std::cin, temp);
    if (!temp.empty()) bpm = std::stoul(temp);

    std::cout << "HP drain rate: ";
    std::getline(std::cin, temp);
    if (!temp.empty()) hpDrain = std::stof(temp);

    std::cout << "Overall difficulty: ";
    std::getline(std::cin, temp);
    if (!temp.empty()) accuracy = std::stof(temp);

    std::cout << "Difficulty name: ";
    std::getline(std::cin, difficultyName);

    std::cout << "Proposed star rating: ";
    std::getline(std::cin, temp);
    if (!temp.empty()) proposedSr = std::stof(temp);

    // audio analysis
    std::cout << "\nStarting audio analysis...\n";
    
    // make the processor jump halfway into the window to prevent blurry timing (~11.6ms resolution at 44.1kHz)
    Analyzer analyzer(1024, 512);
    
    auto decoded = AudioLoader::LoadAudio(audioFile);

    std::vector<AudioFrame> analysisTimeline = analyzer.Analyze(decoded);
    std::cout << "Analysis completed. Generated " << analysisTimeline.size() << " spectrum frames.\n";

    // map configuration
    MapConfig config{
        .Name = name,
        .Author = author,
        .Creator = creator,
        .Source = source,
        .AudioFile = audioFile,
        .DifficultyName = difficultyName,
        .BPM = bpm,
        .Keys = keys,
        .HPDrainRate = hpDrain,
        .OverallDifficulty = accuracy,
        .StarRating = proposedSr,
    };
    
    auto generatedFile = dotosu(config);

    TimingPoint startPoint{
        .OffsetMs = 0,
        .BeatLength = generatedFile.MillisecondsPerBeat,
        .Meter = 4, // standard
        .SampleSet = 0, // default
        .SampleIndex = 0, // default
        .Volume = 100,
        .Uninherited = 1, // 1 indicates this is a timing point (bpm shift)
        .Effects = 0
    };

    generatedFile.AddTimingPoint(startPoint);

    Composer composer;
    auto notes = composer.Compose(config, decoded);

    for (const auto& note : notes) {
        generatedFile.AddHitObject(note.Column, note.HitTime, note.ReleaseTime);
    }

    ManiaDifficultyCalculator calc;
    auto sr = calc.Calculate(generatedFile);

    std::cout << "Generated " << notes.size() << " notes (estimated ~"
               << sr << "* against a target of "
               << config.StarRating << "*).\n";

    std::filesystem::path outPath = std::filesystem::current_path() / "output";
    if (!std::filesystem::exists(outPath)) {
        std::filesystem::create_directories(outPath);
    }
    
    if (generatedFile.Save(outPath)) {
        std::cout << "\nMap generated and packed into standard file hierarchy under /output.\n";
    } else {
        std::cerr << "\nError saving mapped assets.\n";
        return 1;
    }

    return 0;
}