#include "LeoKPathSnapshot.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>

#include <unistd.h>

#include "LeoRouteSnapshot.h"

namespace inet {
namespace leoRouting {

namespace {

constexpr size_t GROUP_PREFIX_WORDS = 6;
constexpr size_t PATH_PREFIX_WORDS = 7;

uint64_t joinUint64(int32_t low, int32_t high)
{
    return static_cast<uint64_t>(static_cast<uint32_t>(low)) |
           (static_cast<uint64_t>(static_cast<uint32_t>(high)) << 32);
}

void appendUint64(std::vector<int32_t>& words, uint64_t value)
{
    words.push_back(static_cast<int32_t>(static_cast<uint32_t>(value)));
    words.push_back(static_cast<int32_t>(static_cast<uint32_t>(value >> 32)));
}

uint64_t doubleBits(double value)
{
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

double bitsDouble(int32_t low, int32_t high)
{
    const uint64_t bits = joinUint64(low, high);
    double value = 0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void appendDouble(std::vector<int32_t>& words, double value)
{
    appendUint64(words, doubleBits(value));
}

uint32_t decodeLittleEndianWord(const unsigned char *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

void writeLittleEndianWord(std::ostream& output, int32_t word)
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

std::vector<int32_t> readWords(const std::filesystem::path& path, uint64_t& bytesRead)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
        throw std::runtime_error("Cannot open endpoint K-path snapshot " + path.string());

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0)
        throw std::runtime_error("Endpoint K-path snapshot is empty: " + path.string());
    if (size % 4 != 0)
        throw std::runtime_error("Endpoint K-path snapshot is not 32-bit aligned: " + path.string());
    if (static_cast<uint64_t>(size) > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        throw std::runtime_error("Endpoint K-path snapshot is too large: " + path.string());

    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char *>(bytes.data()), size);
    if (input.gcount() != size || input.fail())
        throw std::runtime_error("Endpoint K-path snapshot is truncated while reading: " + path.string());

    std::vector<int32_t> words(bytes.size() / 4);
    for (size_t index = 0; index < words.size(); ++index)
        words[index] = static_cast<int32_t>(decodeLittleEndianWord(bytes.data() + index * 4));
    bytesRead = bytes.size();
    return words;
}

uint64_t computeWordHash(const std::vector<int32_t>& words, size_t begin)
{
    uint64_t hash = 1469598103934665603ULL;
    for (size_t index = begin; index < words.size(); ++index) {
        const uint32_t value = static_cast<uint32_t>(words[index]);
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

bool nearlyEqual(double left, double right)
{
    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= 1e-9 * scale;
}

uint64_t normalizedEdgeKey(int32_t first, int32_t second)
{
    if (first > second)
        std::swap(first, second);
    return static_cast<uint64_t>(static_cast<uint32_t>(first)) << 32 |
           static_cast<uint32_t>(second);
}

void validateHeader(const KPathSnapshotHeader& header)
{
    if (header.sequence < 0)
        throw std::runtime_error("Endpoint K-path snapshot sequence must be non-negative");
    if (header.timestampMicros < 0)
        throw std::runtime_error("Endpoint K-path snapshot timestamp must be non-negative");
    if (header.routableNodeCount <= 0 || header.totalNodeCount <= header.routableNodeCount)
        throw std::runtime_error("Endpoint K-path snapshot node dimensions are invalid");
    if (header.maxPathCount <= 0)
        throw std::runtime_error("Endpoint K-path snapshot path count must be positive");
    if (header.maxSharedCoreLinks < -1)
        throw std::runtime_error("Endpoint K-path snapshot shared-link limit is invalid");
    if (header.edgeDisjoint && header.maxSharedCoreLinks != -1)
        throw std::runtime_error("Endpoint K-path snapshot combines incompatible disjoint policies");
    if (header.algorithm != kPathAlgorithmForPolicy(header.edgeDisjoint, header.maxSharedCoreLinks))
        throw std::runtime_error("Endpoint K-path snapshot algorithm does not match its disjoint policy");
    if (!std::isfinite(header.maxRttSpreadMs) || header.maxRttSpreadMs < 0)
        throw std::runtime_error("Endpoint K-path snapshot RTT spread must be finite and non-negative");
}

void validateGroup(const KPathSnapshotHeader& header,
                   const KShortestPathGroup& group,
                   std::unordered_set<uint64_t>& pairKeys)
{
    if (group.requestedPathCount != header.maxPathCount ||
        group.maxRttSpreadMs != header.maxRttSpreadMs ||
        group.edgeDisjoint != header.edgeDisjoint ||
        group.maxSharedCoreLinks != header.maxSharedCoreLinks) {
        throw std::runtime_error("Endpoint K-path group policy does not match its snapshot header");
    }
    if (group.sourceNodeId < header.routableNodeCount ||
        group.sourceNodeId >= header.totalNodeCount ||
        group.destinationNodeId < header.routableNodeCount ||
        group.destinationNodeId >= header.totalNodeCount ||
        group.sourceNodeId >= group.destinationNodeId) {
        throw std::runtime_error("Endpoint K-path group is not a canonical endpoint pair");
    }
    const uint64_t pairKey = kPathEndpointPairKey(group.sourceNodeId, group.destinationNodeId);
    if (!pairKeys.insert(pairKey).second)
        throw std::runtime_error("Duplicate endpoint pair in K-path snapshot");

    const auto validCoreNode = [&](int32_t nodeId) {
        return nodeId == -1 || (nodeId >= 0 && nodeId < header.routableNodeCount);
    };
    if (!validCoreNode(group.coreSourceNodeId) || !validCoreNode(group.coreDestinationNodeId))
        throw std::runtime_error("Endpoint K-path group contains an invalid attached core node");
    if (group.paths.size() > static_cast<size_t>(header.maxPathCount))
        throw std::runtime_error("Endpoint K-path group exceeds the configured path count");
    if (!group.paths.empty() && (group.coreSourceNodeId < 0 || group.coreDestinationNodeId < 0))
        throw std::runtime_error("Endpoint K-path group has paths while an endpoint is disconnected");

    double previousRttMs = -1;
    double accessOneWayDelayMs = -1;
    std::vector<std::vector<int32_t>> pathNodeSequences;
    pathNodeSequences.reserve(group.paths.size());
    std::vector<std::unordered_set<uint64_t>> selectedPathCoreEdges;
    selectedPathCoreEdges.reserve(group.paths.size());
    for (const KShortestPath& path : group.paths) {
        if (path.nodeIds.size() < 3 ||
            path.nodeIds.size() > static_cast<size_t>(header.routableNodeCount) + 2) {
            throw std::runtime_error("Endpoint K-path has an invalid node count");
        }
        if (path.nodeIds.front() != group.sourceNodeId ||
            path.nodeIds.back() != group.destinationNodeId ||
            path.nodeIds[1] != group.coreSourceNodeId ||
            path.nodeIds[path.nodeIds.size() - 2] != group.coreDestinationNodeId) {
            throw std::runtime_error("Endpoint K-path endpoints do not match its group");
        }
        if (std::find(pathNodeSequences.begin(), pathNodeSequences.end(), path.nodeIds) !=
            pathNodeSequences.end()) {
            throw std::runtime_error("Endpoint K-path group contains a duplicate path");
        }
        pathNodeSequences.push_back(path.nodeIds);
        if (!std::isfinite(path.coreOneWayDelayMs) || path.coreOneWayDelayMs < 0 ||
            !std::isfinite(path.oneWayDelayMs) || path.oneWayDelayMs < path.coreOneWayDelayMs ||
            !std::isfinite(path.rttMs) || path.rttMs < 0 ||
            !nearlyEqual(path.rttMs, 2 * path.oneWayDelayMs)) {
            throw std::runtime_error("Endpoint K-path contains invalid delay metadata");
        }
        if (previousRttMs > path.rttMs && !nearlyEqual(previousRttMs, path.rttMs))
            throw std::runtime_error("Endpoint K-paths are not ordered by RTT");
        if (previousRttMs >= 0 &&
            path.rttMs - group.paths.front().rttMs > header.maxRttSpreadMs + 1e-9) {
            throw std::runtime_error("Endpoint K-path exceeds the snapshot RTT range");
        }
        previousRttMs = path.rttMs;

        const double pathAccessDelayMs = path.oneWayDelayMs - path.coreOneWayDelayMs;
        if (accessOneWayDelayMs < 0)
            accessOneWayDelayMs = pathAccessDelayMs;
        else if (!nearlyEqual(accessOneWayDelayMs, pathAccessDelayMs))
            throw std::runtime_error("Endpoint K-paths do not share the same access delay");

        std::unordered_set<int32_t> visitedCoreNodes;
        std::unordered_set<uint64_t> pathCoreEdges;
        for (size_t index = 1; index + 1 < path.nodeIds.size(); ++index) {
            const int32_t nodeId = path.nodeIds[index];
            if (nodeId < 0 || nodeId >= header.routableNodeCount)
                throw std::runtime_error("Endpoint K-path contains a non-core intermediate node");
            if (!visitedCoreNodes.insert(nodeId).second)
                throw std::runtime_error("Endpoint K-path contains a core-node loop");
            if (index + 2 < path.nodeIds.size())
                pathCoreEdges.insert(normalizedEdgeKey(nodeId, path.nodeIds[index + 1]));
        }
        const int32_t maxSharedCoreLinks = header.edgeDisjoint ? 0 : header.maxSharedCoreLinks;
        if (maxSharedCoreLinks >= 0) {
            for (const auto& selectedCoreEdges : selectedPathCoreEdges) {
                int32_t sharedCoreLinks = 0;
                for (uint64_t edge : pathCoreEdges) {
                    if (selectedCoreEdges.count(edge) != 0)
                        sharedCoreLinks++;
                }
                if (sharedCoreLinks > maxSharedCoreLinks) {
                    if (header.edgeDisjoint)
                        throw std::runtime_error("Endpoint K-path group is not core-edge-disjoint");
                    throw std::runtime_error("Endpoint K-path group exceeds its shared core-link limit");
                }
            }
        }
        selectedPathCoreEdges.push_back(std::move(pathCoreEdges));
    }
}

void validateSnapshot(const KPathSnapshotHeader& header,
                      const std::vector<KShortestPathGroup>& groups)
{
    validateHeader(header);
    std::unordered_set<uint64_t> pairKeys;
    pairKeys.reserve(groups.size());
    for (const KShortestPathGroup& group : groups)
        validateGroup(header, group, pairKeys);
}

std::vector<int32_t> encodePayload(const std::vector<KShortestPathGroup>& groups,
                                   int32_t& totalPathCount,
                                   int32_t& totalNodeIdCount)
{
    std::vector<int32_t> payload;
    totalPathCount = 0;
    totalNodeIdCount = 0;
    for (const KShortestPathGroup& group : groups) {
        payload.push_back(group.sourceNodeId);
        payload.push_back(group.destinationNodeId);
        payload.push_back(group.coreSourceNodeId);
        payload.push_back(group.coreDestinationNodeId);
        payload.push_back(static_cast<int32_t>(group.paths.size()));
        payload.push_back(0);
        for (const KShortestPath& path : group.paths) {
            if (path.nodeIds.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
                throw std::runtime_error("Endpoint K-path contains too many nodes");
            payload.push_back(static_cast<int32_t>(path.nodeIds.size()));
            appendDouble(payload, path.coreOneWayDelayMs);
            appendDouble(payload, path.oneWayDelayMs);
            appendDouble(payload, path.rttMs);
            payload.insert(payload.end(), path.nodeIds.begin(), path.nodeIds.end());
            if (totalPathCount == std::numeric_limits<int32_t>::max() ||
                path.nodeIds.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max() - totalNodeIdCount)) {
                throw std::runtime_error("Endpoint K-path snapshot count exceeds its format limits");
            }
            totalPathCount++;
            totalNodeIdCount += static_cast<int32_t>(path.nodeIds.size());
        }
    }
    return payload;
}

} // namespace

std::pair<int32_t, int32_t> normalizeKPathEndpointPair(int32_t first, int32_t second)
{
    if (first == second)
        throw std::invalid_argument("Endpoint K-path pair must contain two different endpoints");
    return first < second ? std::make_pair(first, second) : std::make_pair(second, first);
}

uint64_t kPathEndpointPairKey(int32_t first, int32_t second)
{
    const auto [lower, upper] = normalizeKPathEndpointPair(first, second);
    if (lower < 0)
        throw std::invalid_argument("Endpoint K-path IDs must be non-negative");
    return static_cast<uint64_t>(static_cast<uint32_t>(lower)) << 32 |
           static_cast<uint32_t>(upper);
}

uint64_t computeKPathEndpointPairSetHash(std::vector<std::pair<int32_t, int32_t>> pairs)
{
    for (auto& pair : pairs) {
        pair = normalizeKPathEndpointPair(pair.first, pair.second);
        if (pair.first < 0)
            throw std::invalid_argument("Endpoint K-path pair IDs must be non-negative");
    }
    std::sort(pairs.begin(), pairs.end());
    if (std::adjacent_find(pairs.begin(), pairs.end()) != pairs.end())
        throw std::invalid_argument("Endpoint K-path pair set contains a duplicate pair");

    uint64_t hash = 1469598103934665603ULL;
    auto consume = [&](uint32_t value) {
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ULL;
        }
    };
    consume(static_cast<uint32_t>(pairs.size()));
    for (const auto& [first, second] : pairs) {
        consume(static_cast<uint32_t>(first));
        consume(static_cast<uint32_t>(second));
    }
    return hash;
}

std::string makeKPathSnapshotProfileName(
    KPathAlgorithm algorithm,
    int32_t maxPathCount,
    double maxRttSpreadMs,
    int32_t maxSharedCoreLinks,
    const std::vector<std::pair<int32_t, int32_t>>& pairs)
{
    if (maxPathCount <= 0)
        throw std::invalid_argument("Endpoint K-path profile count must be positive");
    if (!std::isfinite(maxRttSpreadMs) || maxRttSpreadMs < 0)
        throw std::invalid_argument("Endpoint K-path profile RTT spread must be finite and non-negative");
    if (maxSharedCoreLinks < -1)
        throw std::invalid_argument("Endpoint K-path profile shared-link limit must be -1 or non-negative");
    if ((algorithm == KPathAlgorithm::Yen && maxSharedCoreLinks != -1) ||
        (algorithm == KPathAlgorithm::YenOverlapLimited && maxSharedCoreLinks < 0) ||
        (algorithm == KPathAlgorithm::EdgeDisjointMinCostFlow && maxSharedCoreLinks != -1)) {
        throw std::invalid_argument("Endpoint K-path profile algorithm does not match its shared-link policy");
    }
    if (pairs.empty())
        throw std::invalid_argument("Endpoint K-path profile must contain at least one endpoint pair");

    std::ostringstream rtt;
    rtt << std::setprecision(std::numeric_limits<double>::max_digits10) << maxRttSpreadMs;
    std::string rttToken = rtt.str();
    std::replace(rttToken.begin(), rttToken.end(), '.', 'p');
    std::replace(rttToken.begin(), rttToken.end(), '+', '_');

    std::ostringstream profile;
    profile << "v" << K_PATH_SNAPSHOT_VERSION
            << "-" << kPathAlgorithmName(algorithm)
            << "-k" << maxPathCount
            << "-rtt" << rttToken << "ms";
    if (maxSharedCoreLinks >= 0)
        profile << "-shared" << maxSharedCoreLinks;
    profile << "-pairs-" << std::hex << std::setfill('0') << std::setw(16)
            << computeKPathEndpointPairSetHash(pairs);
    return profile.str();
}

uint64_t computeKPathEndpointStateHash(std::vector<KPathEndpointState> states)
{
    std::sort(states.begin(), states.end(), [](const auto& left, const auto& right) {
        return left.endpointNodeId < right.endpointNodeId;
    });
    uint64_t hash = 1469598103934665603ULL;
    auto consume = [&](uint64_t value) {
        for (int byte = 0; byte < 8; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ULL;
        }
    };
    int32_t previousEndpoint = -1;
    for (const KPathEndpointState& state : states) {
        if (state.endpointNodeId < 0 || state.endpointNodeId == previousEndpoint)
            throw std::invalid_argument("Endpoint state list contains an invalid or duplicate endpoint ID");
        if (state.coreNodeId < -1 || !std::isfinite(state.accessOneWayDelayMs) || state.accessOneWayDelayMs < 0)
            throw std::invalid_argument("Endpoint state list contains invalid attachment data");
        if (state.coreNodeId == -1 && state.accessOneWayDelayMs != 0)
            throw std::invalid_argument("A disconnected endpoint state must have zero access delay");
        previousEndpoint = state.endpointNodeId;
        consume(static_cast<uint32_t>(state.endpointNodeId));
        consume(static_cast<uint32_t>(state.coreNodeId));
        consume(doubleBits(state.accessOneWayDelayMs));
    }
    return hash;
}

KShortestPathGroup orientKShortestPathGroup(const KShortestPathGroup& canonicalGroup,
                                            int32_t sourceEndpoint,
                                            int32_t destinationEndpoint,
                                            int32_t requestedPathCount)
{
    if (requestedPathCount <= 0 || requestedPathCount > canonicalGroup.requestedPathCount)
        throw std::invalid_argument("Requested endpoint K-path count is outside the saved range");
    const auto normalized = normalizeKPathEndpointPair(sourceEndpoint, destinationEndpoint);
    if (canonicalGroup.sourceNodeId != normalized.first ||
        canonicalGroup.destinationNodeId != normalized.second) {
        throw std::invalid_argument("Requested endpoints do not match the saved K-path group");
    }

    KShortestPathGroup result = canonicalGroup;
    if (result.paths.size() > static_cast<size_t>(requestedPathCount))
        result.paths.resize(requestedPathCount);
    result.requestedPathCount = requestedPathCount;
    if (sourceEndpoint == canonicalGroup.sourceNodeId)
        return result;

    std::swap(result.sourceNodeId, result.destinationNodeId);
    std::swap(result.coreSourceNodeId, result.coreDestinationNodeId);
    for (KShortestPath& path : result.paths)
        std::reverse(path.nodeIds.begin(), path.nodeIds.end());
    return result;
}

KPathSnapshot readKPathSnapshot(const std::filesystem::path& path)
{
    KPathSnapshot snapshot;
    const std::vector<int32_t> words = readWords(path, snapshot.bytesRead);
    if (words.size() < K_PATH_SNAPSHOT_HEADER_WORDS)
        throw std::runtime_error("Endpoint K-path snapshot header is truncated: " + path.string());
    if (words[0] != K_PATH_SNAPSHOT_MAGIC)
        throw std::runtime_error("Unknown endpoint K-path snapshot magic in " + path.string());
    if (words[1] != K_PATH_SNAPSHOT_VERSION)
        throw std::runtime_error("Unsupported endpoint K-path snapshot version " +
                                 std::to_string(words[1]) + " in " + path.string());
    if (words[2] != K_PATH_SNAPSHOT_HEADER_WORDS)
        throw std::runtime_error("Unsupported endpoint K-path header length in " + path.string());

    KPathSnapshotHeader& header = snapshot.header;
    header.sequence = words[3];
    header.timestampMicros = static_cast<int64_t>(joinUint64(words[4], words[5]));
    header.routableNodeCount = words[6];
    header.totalNodeCount = words[7];
    header.maxPathCount = words[8];
    if (words[9] != 0 && words[9] != 1)
        throw std::runtime_error("Invalid edge-disjoint flag in " + path.string());
    header.edgeDisjoint = words[9] != 0;
    header.maxSharedCoreLinks = words[10];
    header.maxRttSpreadMs = bitsDouble(words[11], words[12]);
    const int32_t pairCount = words[13];
    const int32_t declaredPathCount = words[14];
    const int32_t declaredNodeIdCount = words[15];
    if (words[16] == static_cast<int32_t>(KPathAlgorithm::Yen))
        header.algorithm = KPathAlgorithm::Yen;
    else if (words[16] == static_cast<int32_t>(KPathAlgorithm::EdgeDisjointMinCostFlow))
        header.algorithm = KPathAlgorithm::EdgeDisjointMinCostFlow;
    else if (words[16] == static_cast<int32_t>(KPathAlgorithm::YenOverlapLimited))
        header.algorithm = KPathAlgorithm::YenOverlapLimited;
    else
        throw std::runtime_error("Unknown endpoint K-path algorithm in " + path.string());
    header.routeStateHash = joinUint64(words[17], words[18]);
    header.endpointStateHash = joinUint64(words[19], words[20]);
    header.payloadHash = joinUint64(words[21], words[22]);
    validateHeader(header);
    if (pairCount < 0 || declaredPathCount < 0 || declaredNodeIdCount < 0)
        throw std::runtime_error("Negative endpoint K-path snapshot count in " + path.string());
    const uint64_t endpointCount = static_cast<uint64_t>(header.totalNodeCount - header.routableNodeCount);
    const uint64_t maxPairCount = endpointCount * (endpointCount - 1) / 2;
    if (static_cast<uint64_t>(pairCount) > maxPairCount ||
        static_cast<uint64_t>(declaredPathCount) > static_cast<uint64_t>(pairCount) * header.maxPathCount ||
        static_cast<uint64_t>(declaredNodeIdCount) >
            static_cast<uint64_t>(declaredPathCount) * (header.routableNodeCount + 2)) {
        throw std::runtime_error("Endpoint K-path snapshot counts exceed topology bounds in " + path.string());
    }
    const uint64_t expectedWordCount = K_PATH_SNAPSHOT_HEADER_WORDS +
        static_cast<uint64_t>(pairCount) * GROUP_PREFIX_WORDS +
        static_cast<uint64_t>(declaredPathCount) * PATH_PREFIX_WORDS +
        static_cast<uint64_t>(declaredNodeIdCount);
    if (expectedWordCount != words.size())
        throw std::runtime_error("Endpoint K-path header counts do not match file length in " + path.string());
    if (header.timestampMicros != timestampMicrosFromPath(path))
        throw std::runtime_error("Endpoint K-path header timestamp does not match filename " +
                                 path.filename().string());
    if (computeWordHash(words, K_PATH_SNAPSHOT_HEADER_WORDS) != header.payloadHash)
        throw std::runtime_error("Endpoint K-path payload checksum mismatch in " + path.string());

    size_t offset = K_PATH_SNAPSHOT_HEADER_WORDS;
    int64_t decodedPathCount = 0;
    int64_t decodedNodeIdCount = 0;
    snapshot.groups.reserve(pairCount);
    for (int32_t pairIndex = 0; pairIndex < pairCount; ++pairIndex) {
        if (words.size() - offset < GROUP_PREFIX_WORDS)
            throw std::runtime_error("Truncated endpoint K-path group in " + path.string());
        KShortestPathGroup group;
        group.sourceNodeId = words[offset++];
        group.destinationNodeId = words[offset++];
        group.coreSourceNodeId = words[offset++];
        group.coreDestinationNodeId = words[offset++];
        const int32_t pathCount = words[offset++];
        if (words[offset++] != 0)
            throw std::runtime_error("Nonzero reserved endpoint K-path group word in " + path.string());
        if (pathCount < 0 || pathCount > header.maxPathCount)
            throw std::runtime_error("Invalid endpoint K-path group path count in " + path.string());
        group.requestedPathCount = header.maxPathCount;
        group.maxRttSpreadMs = header.maxRttSpreadMs;
        group.edgeDisjoint = header.edgeDisjoint;
        group.maxSharedCoreLinks = header.maxSharedCoreLinks;
        group.paths.reserve(pathCount);
        for (int32_t pathIndex = 0; pathIndex < pathCount; ++pathIndex) {
            if (words.size() - offset < PATH_PREFIX_WORDS)
                throw std::runtime_error("Truncated endpoint K-path record in " + path.string());
            const int32_t nodeCount = words[offset++];
            if (nodeCount < 3 || nodeCount > header.routableNodeCount + 2)
                throw std::runtime_error("Invalid endpoint K-path node count in " + path.string());
            KShortestPath decodedPath;
            decodedPath.coreOneWayDelayMs = bitsDouble(words[offset], words[offset + 1]);
            offset += 2;
            decodedPath.oneWayDelayMs = bitsDouble(words[offset], words[offset + 1]);
            offset += 2;
            decodedPath.rttMs = bitsDouble(words[offset], words[offset + 1]);
            offset += 2;
            if (static_cast<size_t>(nodeCount) > words.size() - offset)
                throw std::runtime_error("Truncated endpoint K-path node sequence in " + path.string());
            decodedPath.nodeIds.assign(words.begin() + offset, words.begin() + offset + nodeCount);
            offset += nodeCount;
            group.paths.push_back(std::move(decodedPath));
            decodedPathCount++;
            decodedNodeIdCount += nodeCount;
        }
        snapshot.groups.push_back(std::move(group));
    }
    if (offset != words.size())
        throw std::runtime_error("Trailing or incomplete data in endpoint K-path snapshot " + path.string());
    if (decodedPathCount != declaredPathCount || decodedNodeIdCount != declaredNodeIdCount)
        throw std::runtime_error("Endpoint K-path header counts do not match payload in " + path.string());
    validateSnapshot(header, snapshot.groups);
    return snapshot;
}

void writeKPathSnapshotAtomic(const std::filesystem::path& path,
                              KPathSnapshotHeader header,
                              const std::vector<KShortestPathGroup>& groups,
                              bool allowOverwrite)
{
    validateSnapshot(header, groups);
    if (groups.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        throw std::runtime_error("Too many endpoint pairs for a K-path snapshot");
    int32_t totalPathCount = 0;
    int32_t totalNodeIdCount = 0;
    const std::vector<int32_t> payload = encodePayload(groups, totalPathCount, totalNodeIdCount);
    header.payloadHash = computeWordHash(payload, 0);
    if (header.timestampMicros != timestampMicrosFromPath(path))
        throw std::runtime_error("Endpoint K-path header timestamp does not match output filename " +
                                 path.filename().string());

    std::vector<int32_t> headerWords;
    headerWords.reserve(K_PATH_SNAPSHOT_HEADER_WORDS);
    headerWords.push_back(K_PATH_SNAPSHOT_MAGIC);
    headerWords.push_back(K_PATH_SNAPSHOT_VERSION);
    headerWords.push_back(K_PATH_SNAPSHOT_HEADER_WORDS);
    headerWords.push_back(header.sequence);
    appendUint64(headerWords, static_cast<uint64_t>(header.timestampMicros));
    headerWords.push_back(header.routableNodeCount);
    headerWords.push_back(header.totalNodeCount);
    headerWords.push_back(header.maxPathCount);
    headerWords.push_back(header.edgeDisjoint ? 1 : 0);
    headerWords.push_back(header.maxSharedCoreLinks);
    appendDouble(headerWords, header.maxRttSpreadMs);
    headerWords.push_back(static_cast<int32_t>(groups.size()));
    headerWords.push_back(totalPathCount);
    headerWords.push_back(totalNodeIdCount);
    headerWords.push_back(static_cast<int32_t>(header.algorithm));
    appendUint64(headerWords, header.routeStateHash);
    appendUint64(headerWords, header.endpointStateHash);
    appendUint64(headerWords, header.payloadHash);
    if (headerWords.size() != K_PATH_SNAPSHOT_HEADER_WORDS)
        throw std::logic_error("Endpoint K-path header size implementation error");

    std::filesystem::create_directories(path.parent_path());
    if (!allowOverwrite && std::filesystem::exists(path))
        throw std::runtime_error("Refusing to overwrite existing endpoint K-path snapshot " + path.string());
    const std::filesystem::path temporary = path.string() + ".tmp." + std::to_string(getpid());
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            throw std::runtime_error("Cannot create temporary endpoint K-path snapshot " + temporary.string());
        for (int32_t word : headerWords)
            writeLittleEndianWord(output, word);
        for (int32_t word : payload)
            writeLittleEndianWord(output, word);
        output.flush();
        if (!output.good()) {
            output.close();
            std::filesystem::remove(temporary);
            throw std::runtime_error("Failed while writing temporary endpoint K-path snapshot " +
                                     temporary.string());
        }
    }
    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError);
    if (renameError) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot atomically publish endpoint K-path snapshot " + path.string() +
                                 ": " + renameError.message());
    }
}

} // namespace leoRouting
} // namespace inet
