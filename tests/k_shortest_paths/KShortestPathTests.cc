#include "../../src/networklayer/configurator/ipv4/LeoKShortestPaths.h"
#include "../../src/networklayer/configurator/ipv4/LeoKPathSnapshot.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include <unistd.h>

using namespace inet::leoRouting;

namespace {

int testsRun = 0;

std::filesystem::path temporaryDirectory()
{
    return std::filesystem::temp_directory_path() /
           ("leo_k_path_snapshot_tests_" + std::to_string(getpid()));
}

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, const char *message)
{
    if (std::abs(actual - expected) > 1e-9)
        throw std::runtime_error(message);
}

template <typename Function>
void expectFailure(const char *message, Function function)
{
    try {
        function();
    }
    catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

void initializeFinder(KShortestPathFinder& finder)
{
    finder.reset(6,
                 {{0, 1}, {1, 5}, {0, 2}, {2, 5}, {1, 2}, {0, 3}, {3, 5}},
                 {1.0, 1.0, 1.5, 1.5, 0.2, 4.0, 4.0});
}

std::set<std::pair<int32_t, int32_t>> normalizedEdges(const KShortestPath& path)
{
    std::set<std::pair<int32_t, int32_t>> result;
    for (size_t index = 1; index < path.nodeIds.size(); ++index) {
        const int32_t left = std::min(path.nodeIds[index - 1], path.nodeIds[index]);
        const int32_t right = std::max(path.nodeIds[index - 1], path.nodeIds[index]);
        result.emplace(left, right);
    }
    return result;
}

void testOrderedPathsAndRttSpread()
{
    testsRun++;
    KShortestPathFinder finder;
    initializeFinder(finder);
    KShortestPathOptions options;
    options.pathCount = 5;
    options.maxRttSpreadMs = 2.0;
    const std::vector<KShortestPath> paths = finder.findPaths(0, 5, options, 2.0);

    require(!paths.empty(), "non-disjoint K-shortest query returned no paths");
    require(paths.size() < static_cast<size_t>(options.pathCount), "RTT spread did not filter slower paths");
    requireNear(paths.front().coreOneWayDelayMs, 2.0, "shortest core delay is incorrect");
    requireNear(paths.front().oneWayDelayMs, 4.0, "shared access delay was not included");
    requireNear(paths.front().rttMs, 8.0, "RTT was not twice the one-way delay");
    for (size_t index = 1; index < paths.size(); ++index) {
        require(paths[index - 1].rttMs <= paths[index].rttMs, "paths are not ordered by RTT");
        require(paths[index].rttMs - paths.front().rttMs <= options.maxRttSpreadMs + 1e-9,
                "selected path exceeds the RTT spread");
    }
}

void testCoreEdgeDisjointSelection()
{
    testsRun++;
    KShortestPathFinder finder;
    initializeFinder(finder);
    KShortestPathOptions options;
    options.pathCount = 3;
    options.maxRttSpreadMs = 5.0;
    options.edgeDisjoint = true;
    const std::vector<KShortestPath> paths = finder.findPaths(0, 5, options, 2.0);

    require(paths.size() == 2, "edge-disjoint selection returned an unexpected path count");
    const auto firstEdges = normalizedEdges(paths[0]);
    const auto secondEdges = normalizedEdges(paths[1]);
    std::vector<std::pair<int32_t, int32_t>> overlap;
    std::set_intersection(firstEdges.begin(), firstEdges.end(), secondEdges.begin(), secondEdges.end(),
                          std::back_inserter(overlap));
    require(overlap.empty(), "edge-disjoint paths share a core edge");
    requireNear(paths[0].rttMs, 8.0, "first edge-disjoint path RTT is incorrect");
    requireNear(paths[1].rttMs, 10.0, "second edge-disjoint path RTT is incorrect");
}

void testExactEdgeDisjointSelectionAvoidsGreedyTrap()
{
    testsRun++;
    KShortestPathFinder finder;
    finder.reset(6,
                 {{0, 1}, {1, 2}, {2, 5}, {1, 3}, {3, 5}, {0, 4}, {4, 2}},
                 {1.0, 0.1, 1.0, 1.0, 1.0, 1.0, 1.0});
    KShortestPathOptions options;
    options.pathCount = 2;
    options.maxRttSpreadMs = 1.0;
    options.edgeDisjoint = true;
    const std::vector<KShortestPath> paths = finder.findPaths(0, 5, options);

    require(paths.size() == 2,
            "exact edge-disjoint solver missed a valid pair blocked by the ordinary shortest path");
    requireNear(paths[0].coreOneWayDelayMs, 3.0, "first exact edge-disjoint delay is incorrect");
    requireNear(paths[1].coreOneWayDelayMs, 3.0, "second exact edge-disjoint delay is incorrect");
    const auto firstEdges = normalizedEdges(paths[0]);
    const auto secondEdges = normalizedEdges(paths[1]);
    std::vector<std::pair<int32_t, int32_t>> overlap;
    std::set_intersection(firstEdges.begin(), firstEdges.end(), secondEdges.begin(), secondEdges.end(),
                          std::back_inserter(overlap));
    require(overlap.empty(), "exact edge-disjoint paths share a physical edge");
}

void testSameNodeAndValidation()
{
    testsRun++;
    KShortestPathFinder finder;
    initializeFinder(finder);
    KShortestPathOptions options;
    options.pathCount = 4;
    const std::vector<KShortestPath> sameNode = finder.findPaths(2, 2, options, 1.25);
    require(sameNode.size() == 1 && sameNode[0].nodeIds == std::vector<int32_t>({2}),
            "same-node path is incorrect");
    requireNear(sameNode[0].rttMs, 2.5, "same-core-node access RTT is incorrect");

    expectFailure("invalid RTT spread was accepted", [&] {
        KShortestPathOptions invalid = options;
        invalid.maxRttSpreadMs = -1;
        finder.findPaths(0, 5, invalid);
    });
    expectFailure("parallel physical edges were accepted", [&] {
        KShortestPathFinder invalid;
        invalid.reset(3, {{0, 1}, {1, 0}}, {1.0, 1.0});
    });
}

void testDisconnectedTopology()
{
    testsRun++;
    KShortestPathFinder finder;
    finder.reset(4, {{0, 1}, {2, 3}}, {1.0, 1.0});
    KShortestPathOptions options;
    options.pathCount = 3;
    require(finder.findPaths(0, 3, options).empty(),
            "disconnected K-shortest query returned a path");
}

KPathSnapshotHeader makeSnapshotHeader(bool edgeDisjoint = false)
{
    KPathSnapshotHeader header;
    header.sequence = 1;
    header.timestampMicros = 100000;
    header.routableNodeCount = 6;
    header.totalNodeCount = 8;
    header.maxPathCount = 3;
    header.algorithm = kPathAlgorithmForPolicy(edgeDisjoint);
    header.maxRttSpreadMs = 5;
    header.edgeDisjoint = edgeDisjoint;
    header.routeStateHash = 0x123456789abcdef0ULL;
    header.endpointStateHash = computeKPathEndpointStateHash({
        {6, 0, 1.0},
        {7, 5, 1.0},
    });
    return header;
}

KShortestPathGroup makeSnapshotGroup(bool edgeDisjoint = false)
{
    KShortestPathGroup group;
    group.sourceNodeId = 6;
    group.destinationNodeId = 7;
    group.coreSourceNodeId = 0;
    group.coreDestinationNodeId = 5;
    group.requestedPathCount = 3;
    group.maxRttSpreadMs = 5;
    group.edgeDisjoint = edgeDisjoint;
    group.paths = {
        {{6, 0, 1, 5, 7}, 2.0, 4.0, 8.0},
        {{6, 0, 2, 5, 7}, 3.0, 5.0, 10.0},
    };
    return group;
}

void testSnapshotRoundTripAndOrientation()
{
    testsRun++;
    const std::filesystem::path directory = temporaryDirectory();
    std::filesystem::remove_all(directory);
    const std::filesystem::path path = directory / "0.1.bin";
    const KPathSnapshotHeader header = makeSnapshotHeader();
    const KShortestPathGroup group = makeSnapshotGroup();
    writeKPathSnapshotAtomic(path, header, {group}, false);

    const KPathSnapshot decoded = readKPathSnapshot(path);
    require(decoded.header.sequence == header.sequence, "K-path snapshot sequence changed");
    require(decoded.header.algorithm == header.algorithm, "K-path snapshot algorithm changed");
    require(decoded.header.routeStateHash == header.routeStateHash, "K-path route-state hash changed");
    require(decoded.groups.size() == 1 && decoded.groups[0].paths.size() == 2,
            "K-path snapshot group count changed");
    require(decoded.groups[0].paths[1].nodeIds == group.paths[1].nodeIds,
            "K-path snapshot node sequence changed");
    requireNear(decoded.groups[0].paths[1].rttMs, 10.0, "K-path snapshot RTT changed");

    const KShortestPathGroup reversed = orientKShortestPathGroup(decoded.groups[0], 7, 6, 1);
    require(reversed.sourceNodeId == 7 && reversed.destinationNodeId == 6,
            "reverse K-path endpoint orientation is incorrect");
    require(reversed.paths.size() == 1 &&
            reversed.paths[0].nodeIds == std::vector<int32_t>({7, 5, 1, 0, 6}),
            "reverse K-path node sequence is incorrect");
    std::filesystem::remove_all(directory);
}

void testDisconnectedSnapshotGroup()
{
    testsRun++;
    const std::filesystem::path directory = temporaryDirectory();
    std::filesystem::remove_all(directory);
    const std::filesystem::path path = directory / "0.1.bin";
    KPathSnapshotHeader header = makeSnapshotHeader();
    header.endpointStateHash = computeKPathEndpointStateHash({
        {6, 0, 1.0},
        {7, -1, 0.0},
    });
    KShortestPathGroup group = makeSnapshotGroup();
    group.coreDestinationNodeId = -1;
    group.paths.clear();
    writeKPathSnapshotAtomic(path, header, {group}, false);
    const KPathSnapshot decoded = readKPathSnapshot(path);
    require(decoded.groups[0].coreDestinationNodeId == -1 && decoded.groups[0].paths.empty(),
            "disconnected endpoint K-path state changed");
    std::filesystem::remove_all(directory);
}

void testSnapshotValidationFailures()
{
    testsRun++;
    const std::filesystem::path directory = temporaryDirectory();
    std::filesystem::remove_all(directory);
    const std::filesystem::path path = directory / "0.1.bin";
    const KPathSnapshotHeader header = makeSnapshotHeader();
    const KShortestPathGroup group = makeSnapshotGroup();

    expectFailure("duplicate endpoint K-path groups were accepted", [&] {
        writeKPathSnapshotAtomic(path, header, {group, group}, false);
    });
    expectFailure("non-core intermediate K-path node was accepted", [&] {
        KShortestPathGroup invalid = group;
        invalid.paths[0].nodeIds[2] = 6;
        writeKPathSnapshotAtomic(path, header, {invalid}, false);
    });
    expectFailure("mismatched K-path policy was accepted", [&] {
        KShortestPathGroup invalid = group;
        invalid.edgeDisjoint = true;
        writeKPathSnapshotAtomic(path, header, {invalid}, false);
    });
    expectFailure("mismatched K-path algorithm was accepted", [&] {
        KPathSnapshotHeader invalid = header;
        invalid.algorithm = KPathAlgorithm::EdgeDisjointMinCostFlow;
        writeKPathSnapshotAtomic(path, invalid, {group}, false);
    });
    expectFailure("overlapping edge-disjoint K paths were accepted", [&] {
        KPathSnapshotHeader disjointHeader = makeSnapshotHeader(true);
        KShortestPathGroup invalid = makeSnapshotGroup(true);
        invalid.paths[1] = {{6, 0, 1, 2, 5, 7}, 3.0, 5.0, 10.0};
        writeKPathSnapshotAtomic(path, disjointHeader, {invalid}, false);
    });

    writeKPathSnapshotAtomic(path, header, {group}, false);
    std::filesystem::resize_file(path, std::filesystem::file_size(path) - 4);
    expectFailure("truncated endpoint K-path snapshot was accepted", [&] {
        readKPathSnapshot(path);
    });

    std::filesystem::remove(path);
    writeKPathSnapshotAtomic(path, header, {group}, false);
    {
        std::fstream corrupt(path, std::ios::binary | std::ios::in | std::ios::out);
        require(corrupt.is_open(), "could not open endpoint K-path snapshot for corruption test");
        corrupt.seekg(-1, std::ios::end);
        char byte = 0;
        corrupt.read(&byte, 1);
        byte ^= 0x01;
        corrupt.seekp(-1, std::ios::end);
        corrupt.write(&byte, 1);
    }
    expectFailure("corrupt endpoint K-path snapshot was accepted", [&] {
        readKPathSnapshot(path);
    });
    std::filesystem::remove_all(directory);
}

void testEndpointStateHash()
{
    testsRun++;
    const uint64_t ordered = computeKPathEndpointStateHash({{6, 0, 1.25}, {7, 5, 2.5}});
    const uint64_t reversed = computeKPathEndpointStateHash({{7, 5, 2.5}, {6, 0, 1.25}});
    const uint64_t changed = computeKPathEndpointStateHash({{6, 0, 1.25}, {7, 4, 2.5}});
    require(ordered == reversed, "endpoint-state hash depends on input ordering");
    require(ordered != changed, "endpoint-state hash ignored an attachment change");
    expectFailure("duplicate endpoint state was accepted", [&] {
        computeKPathEndpointStateHash({{6, 0, 1.25}, {6, 0, 1.25}});
    });
}

void testSnapshotProfileSeparation()
{
    testsRun++;
    const std::vector<std::pair<int32_t, int32_t>> pairs = {{6, 7}, {8, 9}};
    const std::string yen = makeKPathSnapshotProfileName(KPathAlgorithm::Yen, 5, 5.0, pairs);
    const std::string edgeDisjoint = makeKPathSnapshotProfileName(
        KPathAlgorithm::EdgeDisjointMinCostFlow, 5, 5.0, pairs);
    require(yen != edgeDisjoint, "snapshot profile does not encode the path algorithm");
    require(yen != makeKPathSnapshotProfileName(KPathAlgorithm::Yen, 4, 5.0, pairs),
            "snapshot profile does not encode K");
    require(yen != makeKPathSnapshotProfileName(KPathAlgorithm::Yen, 5, 4.5, pairs),
            "snapshot profile does not encode the RTT spread");
    require(yen != makeKPathSnapshotProfileName(KPathAlgorithm::Yen, 5, 5.0, {{6, 7}}),
            "snapshot profile does not encode the endpoint pair set");
    require(yen.find("v2-yen-k5-rtt5ms-pairs-") == 0,
            "snapshot profile is not readable or versioned");
}

} // namespace

int main()
{
    try {
        testOrderedPathsAndRttSpread();
        testCoreEdgeDisjointSelection();
        testExactEdgeDisjointSelectionAvoidsGreedyTrap();
        testSameNodeAndValidation();
        testDisconnectedTopology();
        testSnapshotRoundTripAndOrientation();
        testDisconnectedSnapshotGroup();
        testSnapshotValidationFailures();
        testEndpointStateHash();
        testSnapshotProfileSeparation();
        std::cout << "PASS: " << testsRun << " K-shortest path tests" << std::endl;
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "FAIL after " << testsRun << " tests: " << error.what() << std::endl;
        return 1;
    }
}
