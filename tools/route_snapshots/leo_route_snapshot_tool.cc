#include "../../src/networklayer/configurator/ipv4/LeoRouteSnapshot.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;
using namespace inet::leoRouting;

namespace {

struct CommonOptions {
    int32_t nodeCount = 0;
    size_t progressEvery = 25;
};

uint64_t fileSize(const fs::path& path)
{
    return static_cast<uint64_t>(fs::file_size(path));
}

std::string humanBytes(uint64_t bytes)
{
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < std::size(units)) {
        value /= 1024.0;
        unit++;
    }
    std::ostringstream output;
    output << std::fixed << std::setprecision(unit == 0 ? 0 : 2) << value << ' ' << units[unit];
    return output.str();
}

int32_t parsePositiveInt(const std::string& value, const std::string& option)
{
    size_t consumed = 0;
    const long parsed = std::stol(value, &consumed);
    if (consumed != value.size() || parsed <= 0 || parsed > std::numeric_limits<int32_t>::max())
        throw std::runtime_error("Invalid " + option + " value: " + value);
    return static_cast<int32_t>(parsed);
}

CommonOptions parseCommonOptions(int argc, char **argv, int firstOption)
{
    CommonOptions options;
    for (int index = firstOption; index < argc; ++index) {
        const std::string option = argv[index];
        if (option == "--node-count") {
            if (++index >= argc)
                throw std::runtime_error("--node-count requires a value");
            options.nodeCount = parsePositiveInt(argv[index], option);
        }
        else if (option == "--progress-every") {
            if (++index >= argc)
                throw std::runtime_error("--progress-every requires a value");
            options.progressEvery = static_cast<size_t>(parsePositiveInt(argv[index], option));
        }
        else {
            throw std::runtime_error("Unknown option: " + option);
        }
    }
    return options;
}

int32_t inferNodeCount(const ParsedSnapshot& snapshot)
{
    if (snapshot.format != SnapshotFormat::V2Full)
        return snapshot.header.destinationCount;
    int32_t maximum = -1;
    for (const RouteRecord& record : snapshot.records)
        maximum = std::max({maximum, record.source, record.destination, record.nextHop});
    if (maximum < 0 || maximum == std::numeric_limits<int32_t>::max())
        throw std::runtime_error("Cannot infer node count from an empty or invalid v2 snapshot");
    return maximum + 1;
}

void writeTextAtomic(const fs::path& path, const std::string& contents)
{
    fs::create_directories(path.parent_path());
    const fs::path temporary = path.string() + ".tmp." + std::to_string(getpid());
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output.is_open())
            throw std::runtime_error("Cannot create " + temporary.string());
        output << contents;
        output.flush();
        if (!output.good()) {
            output.close();
            fs::remove(temporary);
            throw std::runtime_error("Failed while writing " + temporary.string());
        }
    }
    std::error_code error;
    fs::rename(temporary, path, error);
    if (error) {
        fs::remove(temporary);
        throw std::runtime_error("Cannot atomically publish " + path.string() + ": " + error.message());
    }
}

void copyFileAtomic(const fs::path& source, const fs::path& destination)
{
    if (!fs::exists(source) || fs::exists(destination))
        return;
    const fs::path temporary = destination.string() + ".tmp." + std::to_string(getpid());
    fs::copy_file(source, temporary, fs::copy_options::none);
    std::error_code error;
    fs::rename(temporary, destination, error);
    if (error) {
        fs::remove(temporary);
        throw std::runtime_error("Cannot atomically copy " + source.string() + ": " + error.message());
    }
}

void ensureSeparateOutput(const fs::path& inputDirectory, const fs::path& outputDirectory)
{
    const fs::path canonicalInput = fs::weakly_canonical(inputDirectory);
    const fs::path canonicalOutput = fs::weakly_canonical(outputDirectory);
    if (canonicalInput == canonicalOutput)
        throw std::runtime_error("Input and output routing directories must be different");
    fs::create_directories(outputDirectory);
    for (const auto& entry : fs::directory_iterator(outputDirectory)) {
        if (entry.path().extension() == ".bin")
            throw std::runtime_error("Output directory already contains routing snapshots: " +
                                     outputDirectory.string());
    }
}

bool isHighlightedWindow(const fs::path& path)
{
    const std::string filename = path.filename().string();
    return filename == "285.900001.bin" || filename == "286.000001.bin" ||
           filename == "286.100001.bin";
}

int convertCorpus(const fs::path& inputDirectory,
                  const fs::path& outputRoot,
                  const CommonOptions& options)
{
    const std::vector<fs::path> inputFiles = listSnapshotFilesNumeric(inputDirectory);
    if (inputFiles.empty())
        throw std::runtime_error("No .bin snapshots found in " + inputDirectory.string());

    const fs::path outputDirectory = outputRoot / inputDirectory.filename();
    ensureSeparateOutput(inputDirectory, outputDirectory);
    const fs::path incompleteMarker = outputDirectory / ".route-corpus.incomplete";
    const fs::path completeMarker = outputDirectory / ".route-corpus.complete";
    if (fs::exists(completeMarker))
        throw std::runtime_error("Output corpus is already marked complete: " + outputDirectory.string());
    writeTextAtomic(incompleteMarker,
                    "LEO3 conversion in progress\ninput=" + fs::weakly_canonical(inputDirectory).string() + "\n");
    copyFileAtomic(inputDirectory / "idMap.txt", outputDirectory / "idMap.txt");

    uint64_t totalInputBytes = 0;
    uint64_t totalOutputBytes = 0;
    uint64_t totalInputRecords = 0;
    uint64_t totalEffectiveRoutes = 0;
    uint64_t totalDuplicates = 0;
    uint64_t totalDeltaOperations = 0;
    uint64_t totalAdds = 0;
    uint64_t totalSets = 0;
    uint64_t totalDeletes = 0;

    ParsedSnapshot firstSnapshot = readSnapshot(inputFiles.front());
    if (firstSnapshot.format != SnapshotFormat::V2Full)
        throw std::runtime_error("Conversion input must be a v2 full-snapshot corpus");
    const int32_t nodeCount = options.nodeCount > 0 ? options.nodeCount : inferNodeCount(firstSnapshot);
    FullDecodeStats firstStats;
    StableRouteState previousState = StableRouteState::fromFullSnapshot(
        firstSnapshot, nodeCount, nodeCount, &firstStats);
    SnapshotHeader baseHeader = makeBaseHeader(previousState, 0, timestampMicrosFromPath(inputFiles.front()));
    const std::vector<RouteRecord> baseRecords = previousState.effectiveRecords();
    const fs::path firstOutput = outputDirectory / inputFiles.front().filename();
    writeV3SnapshotAtomic(firstOutput, baseHeader, baseRecords);
    previousState.setSnapshotMetadata(baseHeader);

    totalInputBytes += firstSnapshot.bytesRead;
    totalOutputBytes += fileSize(firstOutput);
    totalInputRecords += firstStats.inputRecords;
    totalEffectiveRoutes += firstStats.effectiveRoutes;
    totalDuplicates += firstStats.duplicateRecords;
    std::cout << "[1/" << inputFiles.size() << "] " << inputFiles.front().filename().string()
              << " base routes=" << firstStats.effectiveRoutes
              << " duplicates=" << firstStats.duplicateRecords
              << " input=" << humanBytes(firstSnapshot.bytesRead)
              << " output=" << humanBytes(fileSize(firstOutput)) << std::endl;

    const auto started = std::chrono::steady_clock::now();
    for (size_t index = 1; index < inputFiles.size(); ++index) {
        ParsedSnapshot input = readSnapshot(inputFiles[index]);
        if (input.format != SnapshotFormat::V2Full)
            throw std::runtime_error("Non-v2 input snapshot at " + inputFiles[index].string());
        FullDecodeStats decodeStats;
        StableRouteState currentState = StableRouteState::fromFullSnapshot(
            input, nodeCount, nodeCount, &decodeStats);
        std::vector<RouteRecord> changes = diffRoutes(previousState, currentState);
        for (const RouteRecord& change : changes) {
            const int32_t oldNextHop = previousState.get(change.source, change.destination);
            if (change.nextHop == DELETE_NEXT_HOP)
                totalDeletes++;
            else if (oldNextHop == DELETE_NEXT_HOP)
                totalAdds++;
            else
                totalSets++;
        }
        SnapshotHeader deltaHeader = makeDeltaHeader(previousState, currentState,
                                                     static_cast<int32_t>(index),
                                                     timestampMicrosFromPath(inputFiles[index]));
        const fs::path outputPath = outputDirectory / inputFiles[index].filename();
        writeV3SnapshotAtomic(outputPath, deltaHeader, changes);
        currentState.setSnapshotMetadata(deltaHeader);

        totalInputBytes += input.bytesRead;
        totalOutputBytes += fileSize(outputPath);
        totalInputRecords += decodeStats.inputRecords;
        totalEffectiveRoutes += decodeStats.effectiveRoutes;
        totalDuplicates += decodeStats.duplicateRecords;
        totalDeltaOperations += changes.size();
        previousState = std::move(currentState);

        if ((index + 1) % options.progressEvery == 0 || index + 1 == inputFiles.size() ||
            isHighlightedWindow(inputFiles[index])) {
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cout << '[' << index + 1 << '/' << inputFiles.size() << "] "
                      << inputFiles[index].filename().string()
                      << " changes=" << changes.size()
                      << " duplicates=" << decodeStats.duplicateRecords
                      << " cumulativeInput=" << humanBytes(totalInputBytes)
                      << " cumulativeOutput=" << humanBytes(totalOutputBytes)
                      << " elapsed=" << std::fixed << std::setprecision(1) << elapsed << "s"
                      << std::endl;
        }
    }

    const double redundancy = totalEffectiveRoutes == 0 ? 0.0 :
        100.0 * (1.0 - static_cast<double>(totalDeltaOperations) /
                         static_cast<double>(totalEffectiveRoutes - firstStats.effectiveRoutes));
    std::ostringstream manifest;
    manifest << "format=LEO3\n"
             << "snapshots=" << inputFiles.size() << "\n"
             << "nodeCount=" << nodeCount << "\n"
             << "inputBytes=" << totalInputBytes << "\n"
             << "outputBytes=" << totalOutputBytes << "\n"
             << "inputRecords=" << totalInputRecords << "\n"
             << "deltaOperations=" << totalDeltaOperations << "\n"
             << "adds=" << totalAdds << "\n"
             << "sets=" << totalSets << "\n"
             << "deletes=" << totalDeletes << "\n"
             << "duplicatesCanonicalised=" << totalDuplicates << "\n";
    writeTextAtomic(completeMarker, manifest.str());
    fs::remove(incompleteMarker);

    std::cout << "Conversion complete: " << outputDirectory << '\n'
              << "  snapshots: " << inputFiles.size() << '\n'
              << "  input: " << humanBytes(totalInputBytes) << " (" << totalInputRecords << " records)\n"
              << "  output: " << humanBytes(totalOutputBytes) << '\n'
              << "  delta operations: " << totalDeltaOperations
              << " (ADD=" << totalAdds << ", SET=" << totalSets << ", DELETE=" << totalDeletes << ")\n"
              << "  duplicate v2 records canonicalised: " << totalDuplicates << '\n'
              << "  unchanged-route redundancy: " << std::fixed << std::setprecision(5)
              << redundancy << "%" << std::endl;
    return 0;
}

void reportFirstDifference(const StableRouteState& expected,
                           const StableRouteState& actual,
                           const fs::path& timestamp)
{
    if (expected.sourceCount() != actual.sourceCount() ||
        expected.destinationCount() != actual.destinationCount())
        throw std::runtime_error("Dimension mismatch at " + timestamp.filename().string());
    const auto& expectedRoutes = expected.rawRoutes();
    const auto& actualRoutes = actual.rawRoutes();
    for (size_t index = 0; index < expectedRoutes.size(); ++index) {
        if (expectedRoutes[index] == actualRoutes[index])
            continue;
        const int32_t source = static_cast<int32_t>(index / expected.destinationCount());
        const int32_t destination = static_cast<int32_t>(index % expected.destinationCount());
        throw std::runtime_error("Semantic mismatch at " + timestamp.filename().string() +
                                 ", first differing route (" + std::to_string(source) + "," +
                                 std::to_string(destination) + "): expected " +
                                 std::to_string(expectedRoutes[index]) + ", reconstructed " +
                                 std::to_string(actualRoutes[index]));
    }
}

int verifyCorpus(const fs::path& originalDirectory,
                 const fs::path& convertedDirectory,
                 const CommonOptions& options)
{
    const std::vector<fs::path> originalFiles = listSnapshotFilesNumeric(originalDirectory);
    const std::vector<fs::path> convertedFiles = listSnapshotFilesNumeric(convertedDirectory);
    if (originalFiles.empty())
        throw std::runtime_error("Original corpus contains no snapshots");
    if (originalFiles.size() != convertedFiles.size())
        throw std::runtime_error("Snapshot count mismatch: original=" + std::to_string(originalFiles.size()) +
                                 ", converted=" + std::to_string(convertedFiles.size()));

    ParsedSnapshot firstOriginal = readSnapshot(originalFiles.front());
    const int32_t nodeCount = options.nodeCount > 0 ? options.nodeCount : inferNodeCount(firstOriginal);
    uint64_t originalBytes = 0;
    uint64_t convertedBytes = 0;
    uint64_t originalRecords = 0;
    uint64_t convertedRecords = 0;
    StableRouteState reconstructed;
    const auto started = std::chrono::steady_clock::now();

    for (size_t index = 0; index < originalFiles.size(); ++index) {
        if (originalFiles[index].filename() != convertedFiles[index].filename())
            throw std::runtime_error("Filename mismatch at numeric sequence " + std::to_string(index) +
                                     ": original=" + originalFiles[index].filename().string() +
                                     ", converted=" + convertedFiles[index].filename().string());
        ParsedSnapshot original = index == 0 ? std::move(firstOriginal) : readSnapshot(originalFiles[index]);
        FullDecodeStats decodeStats;
        StableRouteState expected = StableRouteState::fromFullSnapshot(
            original, nodeCount, nodeCount, &decodeStats);
        ParsedSnapshot converted = readSnapshot(convertedFiles[index]);
        if (converted.format != SnapshotFormat::V3)
            throw std::runtime_error("Converted corpus contains a non-v3 snapshot: " + convertedFiles[index].string());
        if (index == 0)
            reconstructed = StableRouteState::fromFullSnapshot(converted, nodeCount, nodeCount);
        else {
            const DeltaPreview preview = reconstructed.validateDelta(converted);
            reconstructed.applyValidatedDelta(converted, preview);
        }

        if (expected.routeCount() != reconstructed.routeCount() ||
            expected.stateHash() != reconstructed.stateHash() ||
            expected.rawRoutes() != reconstructed.rawRoutes())
            reportFirstDifference(expected, reconstructed, originalFiles[index]);

        if (originalFiles[index].filename() == "286.000001.bin") {
            if (decodeStats.duplicateRecords != 2 || expected.get(518, 1354) != 496 ||
                expected.get(1354, 518) != 1332)
                throw std::runtime_error("Known 286.000001 duplicate/last-write-wins check failed");
        }

        originalBytes += original.bytesRead;
        convertedBytes += converted.bytesRead;
        originalRecords += original.records.size();
        convertedRecords += converted.records.size();
        if ((index + 1) % options.progressEvery == 0 || index + 1 == originalFiles.size() ||
            isHighlightedWindow(originalFiles[index])) {
            const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            std::cout << "verified [" << index + 1 << '/' << originalFiles.size() << "] "
                      << originalFiles[index].filename().string()
                      << " effectiveRoutes=" << expected.routeCount()
                      << " deltaRecords=" << converted.records.size()
                      << " elapsed=" << std::fixed << std::setprecision(1) << elapsed << "s"
                      << std::endl;
        }
    }

    std::cout << "Semantic verification passed for all " << originalFiles.size() << " snapshots\n"
              << "  original: " << humanBytes(originalBytes) << " / " << originalRecords << " decoded records\n"
              << "  delta: " << humanBytes(convertedBytes) << " / " << convertedRecords << " decoded records"
              << std::endl;
    return 0;
}

void usage(const char *program)
{
    std::cerr << "Usage:\n"
              << "  " << program << " convert INPUT_DIRECTORY OUTPUT_ROOT [--node-count N] [--progress-every N]\n"
              << "  " << program << " verify ORIGINAL_DIRECTORY CONVERTED_DIRECTORY [--node-count N] [--progress-every N]\n";
}

} // namespace

int main(int argc, char **argv)
{
    try {
        if (argc < 4) {
            usage(argv[0]);
            return 2;
        }
        const std::string command = argv[1];
        const CommonOptions options = parseCommonOptions(argc, argv, 4);
        if (command == "convert")
            return convertCorpus(argv[2], argv[3], options);
        if (command == "verify")
            return verifyCorpus(argv[2], argv[3], options);
        usage(argv[0]);
        return 2;
    }
    catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << std::endl;
        return 1;
    }
}
