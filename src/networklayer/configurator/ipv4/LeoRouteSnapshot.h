//
// Versioned LEO routing snapshot format and stable route-state helpers.
// This file deliberately has no OMNeT++ dependencies so converters and tests
// use exactly the same validation code as the simulator.
//

#ifndef NETWORKLAYER_CONFIGURATOR_IPV4_LEOROUTESNAPSHOT_H_
#define NETWORKLAYER_CONFIGURATOR_IPV4_LEOROUTESNAPSHOT_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace inet {
namespace leoRouting {

constexpr int32_t V2_ROUTE_FILE_MAGIC = 0x4c454f32; // "LEO2"
constexpr int32_t V2_ROUTE_FILE_VERSION = 2;
constexpr int32_t V3_ROUTE_FILE_MAGIC = 0x4c454f33; // "LEO3"
constexpr int32_t V3_ROUTE_FILE_VERSION = 3;
constexpr int32_t V3_HEADER_WORDS = 24;
constexpr int32_t DELETE_NEXT_HOP = -1;

enum class SnapshotFormat {
    V2Full,
    V3,
};

enum class SnapshotKind : int32_t {
    Base = 1,
    Delta = 2,
};

struct RouteRecord {
    int32_t source = -1;
    int32_t destination = -1;
    int32_t nextHop = DELETE_NEXT_HOP;
};

struct SnapshotHeader {
    SnapshotKind kind = SnapshotKind::Base;
    int32_t sequence = -1;
    int32_t baseSequence = -1;
    int32_t previousSequence = -1;
    int32_t sourceCount = 0;
    int32_t destinationCount = 0;
    int64_t timestampMicros = 0;
    uint64_t previousRouteCount = 0;
    uint64_t resultRouteCount = 0;
    uint64_t previousStateHash = 0;
    uint64_t resultStateHash = 0;
    uint64_t baseStateHash = 0;
    uint64_t payloadHash = 0;
};

struct ParsedSnapshot {
    SnapshotFormat format = SnapshotFormat::V2Full;
    SnapshotHeader header;
    std::vector<RouteRecord> records;
    uint64_t bytesRead = 0;
};

struct FullDecodeStats {
    uint64_t inputRecords = 0;
    uint64_t effectiveRoutes = 0;
    uint64_t duplicateRecords = 0;
};

struct DeltaPreview {
    uint64_t resultRouteCount = 0;
    uint64_t resultStateHash = 0;
};

class StableRouteState {
  private:
    int32_t sourceCount_ = 0;
    int32_t destinationCount_ = 0;
    std::vector<int32_t> routes;
    uint64_t routeCount_ = 0;
    uint64_t stateHash_ = 0;
    bool hasSequenceMetadata_ = false;
    int32_t sequence_ = -1;
    int32_t baseSequence_ = -1;
    int64_t timestampMicros_ = 0;
    uint64_t baseStateHash_ = 0;

    size_t routeIndex(int32_t source, int32_t destination) const;
    void setRouteUnchecked(int32_t source, int32_t destination, int32_t nextHop);

  public:
    StableRouteState() = default;
    StableRouteState(int32_t sourceCount, int32_t destinationCount);

    void reset(int32_t sourceCount, int32_t destinationCount);
    int32_t sourceCount() const { return sourceCount_; }
    int32_t destinationCount() const { return destinationCount_; }
    uint64_t routeCount() const { return routeCount_; }
    uint64_t stateHash() const { return stateHash_; }
    bool hasSequenceMetadata() const { return hasSequenceMetadata_; }
    int32_t sequence() const { return sequence_; }
    int32_t baseSequence() const { return baseSequence_; }
    int64_t timestampMicros() const { return timestampMicros_; }
    uint64_t baseStateHash() const { return baseStateHash_; }

    int32_t get(int32_t source, int32_t destination) const;
    void setRoute(int32_t source, int32_t destination, int32_t nextHop);
    const std::vector<int32_t>& rawRoutes() const { return routes; }
    std::vector<RouteRecord> effectiveRecords() const;
    void setSnapshotMetadata(const SnapshotHeader& header);

    DeltaPreview validateDelta(const ParsedSnapshot& snapshot) const;
    void applyValidatedDelta(const ParsedSnapshot& snapshot, const DeltaPreview& preview);

    static StableRouteState fromFullSnapshot(const ParsedSnapshot& snapshot,
                                             int32_t expectedSourceCount,
                                             int32_t expectedDestinationCount,
                                             FullDecodeStats *stats = nullptr);
};

class NextHopInterfaceTable {
  private:
    int32_t trackedSourceCount_ = 0;
    int32_t nodeCount_ = 0;
    std::vector<int32_t> interfaces;
    std::vector<uint8_t> dirtySources_;

    size_t interfaceIndex(int32_t source, int32_t nextHop) const;

  public:
    void reset(int32_t trackedSourceCount, int32_t nodeCount);
    bool set(int32_t source, int32_t nextHop, int32_t interfaceId);
    int32_t get(int32_t source, int32_t nextHop) const;
    bool isDirty(int32_t source) const;
    std::vector<int32_t> dirtySources() const;
    void clearDirty(int32_t source);
    void clearAllDirty();
    void markAllDirty();
};

uint64_t routeContribution(int32_t source, int32_t destination, int32_t nextHop);
uint64_t computePayloadHash(const std::vector<RouteRecord>& records);
int64_t parseTimestampMicros(const std::string& timestamp);
int64_t timestampMicrosFromPath(const std::filesystem::path& path);
std::vector<std::filesystem::path> listSnapshotFilesNumeric(const std::filesystem::path& directory);

ParsedSnapshot readSnapshot(const std::filesystem::path& path);
void writeV3SnapshotAtomic(const std::filesystem::path& path,
                           SnapshotHeader header,
                           const std::vector<RouteRecord>& records,
                           bool allowOverwrite = false);

SnapshotHeader makeBaseHeader(const StableRouteState& state, int32_t sequence, int64_t timestampMicros);
SnapshotHeader makeDeltaHeader(const StableRouteState& previousState,
                               const StableRouteState& resultState,
                               int32_t sequence,
                               int64_t timestampMicros);
std::vector<RouteRecord> diffRoutes(const StableRouteState& previousState,
                                    const StableRouteState& resultState);

std::vector<int> resolveSourceRow(const StableRouteState& routes,
                                  const NextHopInterfaceTable& interfaces,
                                  int32_t source,
                                  size_t liveDestinationCount);
int resolveRoute(const StableRouteState& routes,
                 const NextHopInterfaceTable& interfaces,
                 int32_t source,
                 int32_t destination);

} // namespace leoRouting
} // namespace inet

#endif
