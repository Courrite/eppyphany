#include "Difficulty/ManiaDifficultyCalculator.hpp"
#include "Generation/dotosu.hpp"
#include "Generation/DSProcessor.hpp"
#include "Generation/PatternLibrary.hpp"
#include <filesystem>
#include <iostream>
#include <string>

using namespace eppyphany::Generation;
using namespace eppyphany::Difficulty;

int main() {
    std::string name;
    std::string author;
    std::string creator;
    std::string source;
    std::string difficultyName;
    std::filesystem::path audioFile;
    unsigned int keys = 4;
    unsigned int bpm = 120;

    float hpDrain = 5.0f;
    float accuracy = 5.0f;
    float proposedSr = 0.0f;

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
    DSProcessor processor(1024, 512);
    
    if (!processor.LoadAudio(audioFile)) {
        std::cerr << "Failed to read or decode audio source file. Aborting generation.\n";
        return 1;
    }

    std::vector<AudioFrame> analysisTimeline = processor.Analyze();
    std::cout << "Analysis completed. Generated " << analysisTimeline.size() << " spectrum frames.\n";

    // map configuration
    dotosuFileConfig config{
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

    // note generation
    GeneratorConfig genConfig{
        .Keys = keys,
        // a proposed star rating of 0.0 means the user skipped the prompt,
        // default to 3.5* target because (i think), its the average for
        // the vast majority of casual osu!mania players.
        .TargetStarRating = proposedSr > 0.0f ? proposedSr : 3.5f,
    };

    PatternLibrary library(genConfig);
    std::vector<PlacedNote> notes = library.Generate(analysisTimeline);

    for (const auto& note : notes) {
        generatedFile.AddHitObject(note.Column, note.HitTimeMs, note.ReleaseTimeMs);
    }

    ManiaDifficultyCalculator calc;
    auto sr = calc.Calculate(generatedFile);

    std::cout << "Generated " << notes.size() << " notes (estimated ~"
               << sr << "* against a target of "
               << genConfig.TargetStarRating << "*).\n";

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