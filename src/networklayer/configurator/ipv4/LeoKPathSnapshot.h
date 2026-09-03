//
// Versioned companion snapshots for endpoint-to-endpoint K-shortest paths.
// This codec has no OMNeT++ dependencies so generation and loading share the
// same validation and can be tested independently.
//

#ifndef NETWORKLAYER_CONFIGURATOR_IPV4_LEOKPATHSNAPSHOT_H_
#define NETWORKLAYER_CONFIGURATOR_IPV4_LEOKPATHSNAPSHOT_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "LeoKShortestPaths.h"

namespace inet {
namespace leoRouting {

constexpr int32_t K_PATH_SNAPSHOT_MAGIC = 0x4c454f4b; // "LEOK"
constexpr int32_t K_PATH_SNAPSHOT_VERSION = 3;
constexpr int32_t K_PATH_SNAPSHOT_HEADER_WORDS = 23;

struct KPathSnapshotHeader {
    int32_t sequence = -1;
    int64_t timestampMicros = 0;
    int32_t routableNodeCount = 0;
    int32_t totalNodeCount = 0;
    int32_t maxPathCount = 0;
    KPathAlgorithm algorithm = KPathAlgorithm::Yen;
    double maxRttSpreadMs = 0;
    bool edgeDisjoint = false;
    int32_t maxSharedCoreLinks = -1;
    uint64_t routeStateHash = 0;
    uint64_t endpointStateHash = 0;
    uint64_t payloadHash = 0;
};

struct KPathEndpointState {
    int32_t endpointNodeId = -1;
    int32_t coreNodeId = -1;
    double accessOneWayDelayMs = 0;
};

struct KPathSnapshot {
    KPathSnapshotHeader header;
    std::vector<KShortestPathGroup> groups;
    uint64_t bytesRead = 0;
};

std::pair<int32_t, int32_t> normalizeKPathEndpointPair(int32_t first, int32_t second);
uint64_t kPathEndpointPairKey(int32_t first, int32_t second);
uint64_t computeKPathEndpointPairSetHash(std::vector<std::pair<int32_t, int32_t>> pairs);
std::string makeKPathSnapshotProfileName(KPathAlgorithm algorithm,
                                         int32_t maxPathCount,
                                         double maxRttSpreadMs,
                                         int32_t maxSharedCoreLinks,
                                         const std::vector<std::pair<int32_t, int32_t>>& pairs);
uint64_t computeKPathEndpointStateHash(std::vector<KPathEndpointState> states);

KShortestPathGroup orientKShortestPathGroup(const KShortestPathGroup& canonicalGroup,
                                            int32_t sourceEndpoint,
                                            int32_t destinationEndpoint,
                                            int32_t requestedPathCount);

KPathSnapshot readKPathSnapshot(const std::filesystem::path& path);
void writeKPathSnapshotAtomic(const std::filesystem::path& path,
                              KPathSnapshotHeader header,
                              const std::vector<KShortestPathGroup>& groups,
                              bool allowOverwrite);

} // namespace leoRouting
} // namespace inet

#endif
