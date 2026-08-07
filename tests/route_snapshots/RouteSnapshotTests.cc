#include "../../src/networklayer/configurator/ipv4/LeoRouteSnapshot.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;
using namespace inet::leoRouting;

namespace {

int testsRun = 0;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Function>
void expectFailure(const std::string& name, Function function)
{
    testsRun++;
    try {
        function();
    }
    catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(name + " did not reject invalid input");
}

void writeWord(std::ostream& output, int32_t word)
{
    const uint32_t value = static_cast<uint32_t>(word);
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value),
        static_cast<unsigned char>(value >> 8),
        static_cast<unsigned char>(value >> 16),
        static_cast<unsigned char>(value >> 24),
    };
    output.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
}

void writeV2(const fs::path& path, const std::vector<RouteRecord>& records)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    writeWord(output, V2_ROUTE_FILE_MAGIC);
    writeWord(output, V2_ROUTE_FILE_VERSION);
    for (const RouteRecord& record : records) {
        writeWord(output, record.source);
        writeWord(output, record.destination);
        writeWord(output, record.nextHop);
    }
}

ParsedSnapshot makeParsedDelta(const SnapshotHeader& header, const std::vector<RouteRecord>& records)
{
    ParsedSnapshot snapshot;
    snapshot.format = SnapshotFormat::V3;
    snapshot.header = header;
    snapshot.records = records;
    return snapshot;
}

void testV2AndDuplicateLastWriteWins(const fs::path& directory)
{
    testsRun++;
    const fs::path path = directory / "1.0.bin";
    writeV2(path, {{0, 1, 2}, {0, 1, 1}, {1, 2, 0}});
    ParsedSnapshot snapshot = readSnapshot(path);
    require(snapshot.format == SnapshotFormat::V2Full, "v2 format was not detected");
    FullDecodeStats stats;
    StableRouteState state = StableRouteState::fromFullSnapshot(snapshot, 3, 3, &stats);
    require(state.get(0, 1) == 1, "v2 last-write-wins was not preserved");
    require(state.get(1, 2) == 0, "v2 route was not decoded");
    require(stats.inputRecords == 3 && stats.effectiveRoutes == 2 && stats.duplicateRecords == 1,
            "v2 decode statistics are incorrect");
}

void testBaseDeltaSetAddDelete(const fs::path& directory)
{
    testsRun++;
    StableRouteState base(3, 3);
    base.setRoute(0, 1, 1);
    base.setRoute(1, 2, 2);
    SnapshotHeader baseHeader = makeBaseHeader(base, 0, 100000);
    const fs::path basePath = directory / "0.1.bin";
    writeV3SnapshotAtomic(basePath, baseHeader, base.effectiveRecords());

    ParsedSnapshot parsedBase = readSnapshot(basePath);
    StableRouteState reconstructed = StableRouteState::fromFullSnapshot(parsedBase, 3, 3);
    StableRouteState expected = reconstructed;
    expected.setRoute(0, 1, 2);                 // SET
    expected.setRoute(0, 2, 1);                 // ADD
    expected.setRoute(1, 2, DELETE_NEXT_HOP);   // DELETE

    const std::vector<RouteRecord> changes = diffRoutes(reconstructed, expected);
    require(changes.size() == 3, "SET/ADD/DELETE diff count is incorrect");
    SnapshotHeader deltaHeader = makeDeltaHeader(reconstructed, expected, 1, 200000);
    const fs::path deltaPath = directory / "0.2.bin";
    writeV3SnapshotAtomic(deltaPath, deltaHeader, changes);
    ParsedSnapshot parsedDelta = readSnapshot(deltaPath);
    DeltaPreview preview = reconstructed.validateDelta(parsedDelta);
    reconstructed.applyValidatedDelta(parsedDelta, preview);
    require(reconstructed.rawRoutes() == expected.rawRoutes(), "base plus delta reconstruction differs");
    require(reconstructed.get(0, 1) == 2, "SET was not applied");
    require(reconstructed.get(0, 2) == 1, "ADD was not applied");
    require(reconstructed.get(1, 2) == DELETE_NEXT_HOP, "DELETE was not applied");

    expectFailure("missing base", [&] {
        StableRouteState noBase(3, 3);
        noBase.validateDelta(parsedDelta);
    });
    expectFailure("skipped sequence", [&] {
        ParsedSnapshot skipped = parsedDelta;
        skipped.header.sequence = 2;
        skipped.header.previousSequence = 1;
        StableRouteState loadedBase = StableRouteState::fromFullSnapshot(parsedBase, 3, 3);
        loadedBase.validateDelta(skipped);
    });
    expectFailure("bad delta node id", [&] {
        ParsedSnapshot badNode = parsedDelta;
        badNode.records.front().source = 3;
        StableRouteState loadedBase = StableRouteState::fromFullSnapshot(parsedBase, 3, 3);
        loadedBase.validateDelta(badNode);
    });
    expectFailure("duplicate delta key", [&] {
        ParsedSnapshot duplicate = parsedDelta;
        duplicate.records.push_back(duplicate.records.front());
        StableRouteState loadedBase = StableRouteState::fromFullSnapshot(parsedBase, 3, 3);
        loadedBase.validateDelta(duplicate);
    });
}

void testMalformedAndTruncatedFiles(const fs::path& directory)
{
    expectFailure("malformed v2 record", [&] {
        const fs::path malformed = directory / "2.0.bin";
        std::ofstream output(malformed, std::ios::binary | std::ios::trunc);
        writeWord(output, V2_ROUTE_FILE_MAGIC);
        writeWord(output, V2_ROUTE_FILE_VERSION);
        writeWord(output, 0);
        writeWord(output, 1);
        output.close();
        readSnapshot(malformed);
    });

    expectFailure("bad v2 node id", [&] {
        const fs::path badNode = directory / "2.1.bin";
        writeV2(badNode, {{3, 1, 2}});
        ParsedSnapshot parsed = readSnapshot(badNode);
        StableRouteState::fromFullSnapshot(parsed, 3, 3);
    });

    expectFailure("truncated v3 file", [&] {
        StableRouteState state(2, 2);
        state.setRoute(0, 1, 1);
        SnapshotHeader header = makeBaseHeader(state, 0, 2200000);
        const fs::path valid = directory / "2.2.bin";
        writeV3SnapshotAtomic(valid, header, state.effectiveRecords());
        std::ifstream input(valid, std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        bytes.pop_back();
        const fs::path truncated = directory / "2.3.bin";
        std::ofstream output(truncated, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), bytes.size());
        output.close();
        readSnapshot(truncated);
    });

    expectFailure("corrupt v3 payload", [&] {
        StableRouteState state(2, 2);
        state.setRoute(0, 1, 1);
        SnapshotHeader header = makeBaseHeader(state, 0, 2400000);
        const fs::path valid = directory / "2.4.bin";
        writeV3SnapshotAtomic(valid, header, state.effectiveRecords());
        std::fstream file(valid, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(V3_HEADER_WORDS * 4 + 8);
        const char corrupt = 0;
        file.write(&corrupt, 1);
        file.close();
        readSnapshot(valid);
    });
}

void testPerSourceInterfaceDirtyResolution()
{
    testsRun++;
    StableRouteState routes(2, 4);
    routes.setRoute(0, 2, 1);
    routes.setRoute(1, 2, 3);
    NextHopInterfaceTable interfaces;
    interfaces.reset(2, 4);
    interfaces.set(0, 1, 10);
    interfaces.set(1, 3, 20);
    interfaces.clearAllDirty();

    require(resolveSourceRow(routes, interfaces, 0, 4)[2] == 10, "source 0 initial interface resolution failed");
    require(resolveSourceRow(routes, interfaces, 1, 4)[2] == 20, "source 1 initial interface resolution failed");
    interfaces.set(0, 1, 11);
    const std::vector<int32_t> dirty = interfaces.dirtySources();
    require(dirty.size() == 1 && dirty.front() == 0, "interface change dirtied the wrong source rows");
    require(resolveSourceRow(routes, interfaces, 0, 4)[2] == 11, "dirty source row was not re-resolved");
    require(resolveSourceRow(routes, interfaces, 1, 4)[2] == 20, "clean source row changed unexpectedly");
    interfaces.clearDirty(0);
    require(interfaces.dirtySources().empty(), "dirty source row was not cleared");
    interfaces.set(0, 1, -1);
    require(interfaces.isDirty(0) && !interfaces.isDirty(1), "interface removal dirtied the wrong rows");
    expectFailure("removed next-hop interface", [&] { resolveSourceRow(routes, interfaces, 0, 4); });
}

void testNumericTimestampOrdering(const fs::path& directory)
{
    testsRun++;
    const fs::path orderDirectory = directory / "numeric-order";
    fs::create_directories(orderDirectory);
    writeV2(orderDirectory / "10.bin", {});
    writeV2(orderDirectory / "2.bin", {});
    writeV2(orderDirectory / "1.5.bin", {});
    const std::vector<fs::path> ordered = listSnapshotFilesNumeric(orderDirectory);
    require(ordered.size() == 3, "numeric timestamp listing omitted files");
    require(ordered[0].filename() == "1.5.bin" && ordered[1].filename() == "2.bin" &&
            ordered[2].filename() == "10.bin", "snapshot filenames were sorted lexically instead of numerically");
}

void testReal286Window(const fs::path& corpus)
{
    testsRun++;
    const fs::path p0 = corpus / "285.900001.bin";
    const fs::path p1 = corpus / "286.000001.bin";
    const fs::path p2 = corpus / "286.100001.bin";
    require(fs::exists(p0) && fs::exists(p1) && fs::exists(p2), "real 286-second test files are missing");

    FullDecodeStats stats0, stats1, stats2;
    StableRouteState s0 = StableRouteState::fromFullSnapshot(readSnapshot(p0), 1684, 1684, &stats0);
    StableRouteState s1 = StableRouteState::fromFullSnapshot(readSnapshot(p1), 1684, 1684, &stats1);
    StableRouteState s2 = StableRouteState::fromFullSnapshot(readSnapshot(p2), 1684, 1684, &stats2);
    require(stats0.duplicateRecords == 0 && stats1.duplicateRecords == 2 && stats2.duplicateRecords == 0,
            "unexpected duplicate counts around 286 seconds");
    require(s1.get(518, 1354) == 496, "last-write-wins failed for route (518,1354)");
    require(s1.get(1354, 518) == 1332, "last-write-wins failed for route (1354,518)");

    SnapshotHeader h0 = makeBaseHeader(s0, 0, timestampMicrosFromPath(p0));
    s0.setSnapshotMetadata(h0);
    std::vector<RouteRecord> d1 = diffRoutes(s0, s1);
    SnapshotHeader h1 = makeDeltaHeader(s0, s1, 1, timestampMicrosFromPath(p1));
    ParsedSnapshot delta1 = makeParsedDelta(h1, d1);
    StableRouteState reconstructed = s0;
    reconstructed.applyValidatedDelta(delta1, reconstructed.validateDelta(delta1));
    require(reconstructed.rawRoutes() == s1.rawRoutes(), "285.9 -> 286.0 reconstruction failed");

    s1.setSnapshotMetadata(h1);
    std::vector<RouteRecord> d2 = diffRoutes(s1, s2);
    SnapshotHeader h2 = makeDeltaHeader(s1, s2, 2, timestampMicrosFromPath(p2));
    ParsedSnapshot delta2 = makeParsedDelta(h2, d2);
    reconstructed.applyValidatedDelta(delta2, reconstructed.validateDelta(delta2));
    require(reconstructed.rawRoutes() == s2.rawRoutes(), "286.0 -> 286.1 reconstruction failed");
    std::cout << "real-window changes: 285.9->286.0=" << d1.size()
              << ", 286.0->286.1=" << d2.size() << std::endl;
}

} // namespace

int main(int argc, char **argv)
{
    const fs::path temporary = fs::temp_directory_path() /
        ("leo-route-snapshot-tests-" + std::to_string(getpid()));
    try {
        fs::create_directories(temporary);
        testV2AndDuplicateLastWriteWins(temporary);
        testBaseDeltaSetAddDelete(temporary);
        testMalformedAndTruncatedFiles(temporary);
        testPerSourceInterfaceDirtyResolution();
        testNumericTimestampOrdering(temporary);
        if (argc == 2)
            testReal286Window(argv[1]);
        else if (argc > 2)
            throw std::runtime_error("Usage: RouteSnapshotTests [v2-corpus-directory]");
        fs::remove_all(temporary);
        std::cout << "PASS: " << testsRun << " route snapshot tests" << std::endl;
        return 0;
    }
    catch (const std::exception& error) {
        fs::remove_all(temporary);
        std::cerr << "FAIL after " << testsRun << " tests: " << error.what() << std::endl;
        return 1;
    }
}
