#include "LeoKShortestPaths.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace inet {
namespace leoRouting {

namespace {

constexpr double DISTANCE_EPSILON_MS = 1e-10;

void throwIgraphError(const char *operation, igraph_error_t error)
{
    throw std::runtime_error(std::string(operation) + " failed: " + igraph_strerror(error));
}

uint64_t normalizedEdgeKey(int32_t first, int32_t second)
{
    if (first > second)
        std::swap(first, second);
    return static_cast<uint64_t>(static_cast<uint32_t>(first)) << 32 |
           static_cast<uint32_t>(second);
}

struct CandidatePath {
    std::vector<int32_t> nodeIds;
    double coreOneWayDelayMs = 0;
};

class VectorIntListOwner {
  private:
    igraph_vector_int_list_t value;

  public:
    VectorIntListOwner()
    {
        const igraph_error_t error = igraph_vector_int_list_init(&value, 0);
        if (error != IGRAPH_SUCCESS)
            throwIgraphError("igraph_vector_int_list_init", error);
    }

    ~VectorIntListOwner()
    {
        igraph_vector_int_list_destroy(&value);
    }

    VectorIntListOwner(const VectorIntListOwner&) = delete;
    VectorIntListOwner& operator=(const VectorIntListOwner&) = delete;

    igraph_vector_int_list_t *get() { return &value; }
};

struct ResidualArc {
    int32_t destination = -1;
    size_t reverseIndex = 0;
    int32_t capacity = 0;
    int32_t initialCapacity = 0;
    double costMs = 0;
};

struct ArcReference {
    int32_t source = -1;
    size_t index = 0;
};

class MinCostFlowNetwork {
  private:
    std::vector<std::vector<ResidualArc>> adjacency;
    std::vector<double> potentials;

  public:
    explicit MinCostFlowNetwork(size_t nodeCount) : adjacency(nodeCount), potentials(nodeCount, 0) {}

    ArcReference addArc(int32_t source, int32_t destination, int32_t capacity, double costMs)
    {
        if (source < 0 || destination < 0 ||
            source >= static_cast<int32_t>(adjacency.size()) ||
            destination >= static_cast<int32_t>(adjacency.size()) || source == destination) {
            throw std::logic_error("Invalid min-cost-flow arc endpoint");
        }
        const size_t forwardIndex = adjacency[source].size();
        const size_t reverseIndex = adjacency[destination].size();
        adjacency[source].push_back({destination, reverseIndex, capacity, capacity, costMs});
        adjacency[destination].push_back({source, forwardIndex, 0, 0, -costMs});
        return {source, forwardIndex};
    }

    int32_t flow(const ArcReference& reference) const
    {
        const ResidualArc& arc = adjacency.at(reference.source).at(reference.index);
        return arc.initialCapacity - arc.capacity;
    }

    bool augmentOneUnit(int32_t source, int32_t destination)
    {
        const double infinity = std::numeric_limits<double>::infinity();
        std::vector<double> distances(adjacency.size(), infinity);
        std::vector<int32_t> previousNode(adjacency.size(), -1);
        std::vector<size_t> previousArc(adjacency.size(), 0);
        using QueueEntry = std::pair<double, int32_t>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
        distances[source] = 0;
        queue.emplace(0, source);

        while (!queue.empty()) {
            const auto [distance, node] = queue.top();
            queue.pop();
            if (distance > distances[node] + DISTANCE_EPSILON_MS)
                continue;

            for (size_t arcIndex = 0; arcIndex < adjacency[node].size(); ++arcIndex) {
                const ResidualArc& arc = adjacency[node][arcIndex];
                if (arc.capacity <= 0)
                    continue;
                double reducedCost = arc.costMs + potentials[node] - potentials[arc.destination];
                if (reducedCost < 0) {
                    if (reducedCost < -DISTANCE_EPSILON_MS)
                        throw std::logic_error("Negative reduced cost in edge-disjoint min-cost flow");
                    reducedCost = 0;
                }
                const double candidateDistance = distance + reducedCost;
                if (candidateDistance + DISTANCE_EPSILON_MS >= distances[arc.destination])
                    continue;
                distances[arc.destination] = candidateDistance;
                previousNode[arc.destination] = node;
                previousArc[arc.destination] = arcIndex;
                queue.emplace(candidateDistance, arc.destination);
            }
        }

        if (!std::isfinite(distances[destination]))
            return false;
        for (size_t node = 0; node < adjacency.size(); ++node) {
            if (std::isfinite(distances[node]))
                potentials[node] += distances[node];
        }

        for (int32_t node = destination; node != source; node = previousNode[node]) {
            const int32_t predecessor = previousNode[node];
            if (predecessor < 0)
                throw std::logic_error("Incomplete min-cost-flow predecessor chain");
            ResidualArc& arc = adjacency[predecessor][previousArc[node]];
            if (arc.capacity <= 0)
                throw std::logic_error("Min-cost-flow predecessor arc has no capacity");
            arc.capacity--;
            adjacency[node][arc.reverseIndex].capacity++;
        }
        return true;
    }
};

struct PhysicalEdgeGadget {
    int32_t first = -1;
    int32_t second = -1;
    double weightMs = 0;
    ArcReference enterFromFirst;
    ArcReference enterFromSecond;
    ArcReference central;
    ArcReference leaveToFirst;
    ArcReference leaveToSecond;
};

struct DirectedPhysicalEdge {
    int32_t source = -1;
    int32_t destination = -1;
    int32_t physicalEdgeId = -1;
    double weightMs = 0;
    bool active = true;
};

std::vector<CandidatePath> decomposeFlow(
    const MinCostFlowNetwork& network,
    const std::vector<PhysicalEdgeGadget>& gadgets,
    int32_t originalNodeCount,
    int32_t source,
    int32_t destination,
    int32_t flowValue)
{
    std::vector<DirectedPhysicalEdge> selectedEdges;
    selectedEdges.reserve(gadgets.size());
    for (size_t edgeIndex = 0; edgeIndex < gadgets.size(); ++edgeIndex) {
        const PhysicalEdgeGadget& gadget = gadgets[edgeIndex];
        if (network.flow(gadget.central) == 0)
            continue;
        const bool enteredFromFirst = network.flow(gadget.enterFromFirst) == 1;
        const bool enteredFromSecond = network.flow(gadget.enterFromSecond) == 1;
        const bool leftToFirst = network.flow(gadget.leaveToFirst) == 1;
        const bool leftToSecond = network.flow(gadget.leaveToSecond) == 1;
        if (enteredFromFirst == enteredFromSecond || leftToFirst == leftToSecond)
            throw std::logic_error("Invalid physical-edge flow in edge-disjoint solver");
        const int32_t edgeSource = enteredFromFirst ? gadget.first : gadget.second;
        const int32_t edgeDestination = leftToFirst ? gadget.first : gadget.second;
        if (edgeSource == edgeDestination)
            throw std::logic_error("Edge-disjoint solver produced a physical-edge circulation");
        selectedEdges.push_back({edgeSource, edgeDestination, static_cast<int32_t>(edgeIndex),
                                 gadget.weightMs, true});
    }

    std::vector<std::vector<size_t>> outgoing(originalNodeCount);
    for (size_t index = 0; index < selectedEdges.size(); ++index)
        outgoing[selectedEdges[index].source].push_back(index);
    for (auto& edgeIndexes : outgoing) {
        std::sort(edgeIndexes.begin(), edgeIndexes.end(), [&](size_t left, size_t right) {
            return selectedEdges[left].physicalEdgeId < selectedEdges[right].physicalEdgeId;
        });
    }

    std::vector<CandidatePath> paths;
    paths.reserve(flowValue);
    for (int32_t pathIndex = 0; pathIndex < flowValue; ++pathIndex) {
        const double infinity = std::numeric_limits<double>::infinity();
        std::vector<double> distances(originalNodeCount, infinity);
        std::vector<int32_t> previousEdge(originalNodeCount, -1);
        using QueueEntry = std::pair<double, int32_t>;
        std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
        distances[source] = 0;
        queue.emplace(0, source);
        while (!queue.empty()) {
            const auto [distance, node] = queue.top();
            queue.pop();
            if (distance > distances[node] + DISTANCE_EPSILON_MS)
                continue;
            for (size_t selectedIndex : outgoing[node]) {
                const DirectedPhysicalEdge& edge = selectedEdges[selectedIndex];
                if (!edge.active)
                    continue;
                const double candidateDistance = distance + edge.weightMs;
                if (candidateDistance + DISTANCE_EPSILON_MS >= distances[edge.destination])
                    continue;
                distances[edge.destination] = candidateDistance;
                previousEdge[edge.destination] = static_cast<int32_t>(selectedIndex);
                queue.emplace(candidateDistance, edge.destination);
            }
        }
        if (!std::isfinite(distances[destination]))
            throw std::logic_error("Could not decompose edge-disjoint min-cost flow into paths");

        CandidatePath path;
        path.coreOneWayDelayMs = distances[destination];
        for (int32_t node = destination; node != source;) {
            const int32_t selectedIndex = previousEdge[node];
            if (selectedIndex < 0)
                throw std::logic_error("Incomplete edge-disjoint path predecessor chain");
            DirectedPhysicalEdge& edge = selectedEdges[selectedIndex];
            path.nodeIds.push_back(node);
            edge.active = false;
            node = edge.source;
        }
        path.nodeIds.push_back(source);
        std::reverse(path.nodeIds.begin(), path.nodeIds.end());
        paths.push_back(std::move(path));
    }
    return paths;
}

std::vector<CandidatePath> findEdgeDisjointCandidates(
    int32_t nodeCount,
    const std::vector<std::pair<int32_t, int32_t>>& edges,
    const std::vector<double>& weightsMs,
    int32_t source,
    int32_t destination,
    int32_t pathCount)
{
    if (edges.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max() - nodeCount) / 2))
        throw std::overflow_error("Edge-disjoint expanded topology is too large");
    const int32_t expandedNodeCount = nodeCount + static_cast<int32_t>(edges.size() * 2);
    MinCostFlowNetwork network(expandedNodeCount);
    std::vector<PhysicalEdgeGadget> gadgets;
    gadgets.reserve(edges.size());
    // Both directions pass through one unit-capacity central arc, so a physical
    // link can be consumed by at most one path regardless of traversal direction.
    for (size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex) {
        const auto [first, second] = edges[edgeIndex];
        const int32_t edgeInput = nodeCount + static_cast<int32_t>(edgeIndex * 2);
        const int32_t edgeOutput = edgeInput + 1;
        PhysicalEdgeGadget gadget;
        gadget.first = first;
        gadget.second = second;
        gadget.weightMs = weightsMs[edgeIndex];
        gadget.enterFromFirst = network.addArc(first, edgeInput, 1, 0);
        gadget.enterFromSecond = network.addArc(second, edgeInput, 1, 0);
        gadget.central = network.addArc(edgeInput, edgeOutput, 1, weightsMs[edgeIndex]);
        gadget.leaveToFirst = network.addArc(edgeOutput, first, 1, 0);
        gadget.leaveToSecond = network.addArc(edgeOutput, second, 1, 0);
        gadgets.push_back(gadget);
    }

    int32_t flowValue = 0;
    while (flowValue < pathCount && network.augmentOneUnit(source, destination))
        flowValue++;
    return decomposeFlow(network, gadgets, nodeCount, source, destination, flowValue);
}

bool candidateLess(const CandidatePath& left, const CandidatePath& right)
{
    if (left.coreOneWayDelayMs != right.coreOneWayDelayMs)
        return left.coreOneWayDelayMs < right.coreOneWayDelayMs;
    return left.nodeIds < right.nodeIds;
}

void sortAndDeduplicateCandidates(std::vector<CandidatePath>& candidates)
{
    std::sort(candidates.begin(), candidates.end(), candidateLess);
    candidates.erase(std::unique(candidates.begin(), candidates.end(), [](const auto& left, const auto& right) {
        return left.nodeIds == right.nodeIds;
    }), candidates.end());
}

std::vector<uint64_t> normalizedPathEdges(const CandidatePath& path)
{
    std::vector<uint64_t> edges;
    if (path.nodeIds.size() < 2)
        return edges;
    edges.reserve(path.nodeIds.size() - 1);
    for (size_t index = 1; index < path.nodeIds.size(); ++index)
        edges.push_back(normalizedEdgeKey(path.nodeIds[index - 1], path.nodeIds[index]));
    std::sort(edges.begin(), edges.end());
    return edges;
}

bool sharesAtMost(const std::vector<uint64_t>& candidateEdges,
                  const std::vector<std::vector<uint64_t>>& selectedPathEdges,
                  int32_t maxSharedCoreLinks)
{
    for (const auto& selectedEdges : selectedPathEdges) {
        int32_t sharedLinks = 0;
        size_t candidateIndex = 0;
        size_t selectedIndex = 0;
        while (candidateIndex < candidateEdges.size() && selectedIndex < selectedEdges.size()) {
            if (candidateEdges[candidateIndex] < selectedEdges[selectedIndex])
                candidateIndex++;
            else if (selectedEdges[selectedIndex] < candidateEdges[candidateIndex])
                selectedIndex++;
            else {
                sharedLinks++;
                if (sharedLinks > maxSharedCoreLinks)
                    return false;
                candidateIndex++;
                selectedIndex++;
            }
        }
    }
    return true;
}

std::vector<KShortestPath> selectRttRange(const std::vector<CandidatePath>& candidates,
                                          int32_t pathCount,
                                          double maxRttSpreadMs,
                                          double sharedAccessOneWayDelayMs,
                                          int32_t maxSharedCoreLinks = -1)
{
    if (candidates.empty())
        return {};

    const double shortestRttMs = 2 * (candidates.front().coreOneWayDelayMs + sharedAccessOneWayDelayMs);
    std::vector<KShortestPath> selected;
    std::vector<std::vector<uint64_t>> selectedPathEdges;
    selected.reserve(std::min(static_cast<size_t>(pathCount), candidates.size()));
    selectedPathEdges.reserve(std::min(static_cast<size_t>(pathCount), candidates.size()));
    for (const CandidatePath& candidate : candidates) {
        const double oneWayDelayMs = candidate.coreOneWayDelayMs + sharedAccessOneWayDelayMs;
        const double rttMs = 2 * oneWayDelayMs;
        if (rttMs - shortestRttMs > maxRttSpreadMs + DISTANCE_EPSILON_MS)
            break;
        std::vector<uint64_t> candidateEdges;
        if (maxSharedCoreLinks >= 0) {
            candidateEdges = normalizedPathEdges(candidate);
            if (!sharesAtMost(candidateEdges, selectedPathEdges, maxSharedCoreLinks))
                continue;
        }
        selected.push_back({candidate.nodeIds, candidate.coreOneWayDelayMs, oneWayDelayMs, rttMs});
        if (maxSharedCoreLinks >= 0)
            selectedPathEdges.push_back(std::move(candidateEdges));
        if (selected.size() == static_cast<size_t>(pathCount))
            break;
    }
    return selected;
}

std::vector<CandidatePath> findYenCandidates(const igraph_t *topology,
                                             const igraph_vector_t *edgeWeightsMs,
                                             int32_t source,
                                             int32_t destination,
                                             int32_t candidateCount,
                                             bool& exhausted)
{
    VectorIntListOwner vertexPaths;
    VectorIntListOwner edgePaths;
    const igraph_error_t error = igraph_get_k_shortest_paths(topology, edgeWeightsMs,
                                                             vertexPaths.get(), edgePaths.get(),
                                                             candidateCount, source, destination, IGRAPH_ALL);
    if (error != IGRAPH_SUCCESS)
        throwIgraphError("igraph_get_k_shortest_paths", error);

    const igraph_integer_t vertexPathCount = igraph_vector_int_list_size(vertexPaths.get());
    const igraph_integer_t edgePathCount = igraph_vector_int_list_size(edgePaths.get());
    if (vertexPathCount != edgePathCount)
        throw std::runtime_error("igraph returned different vertex-path and edge-path counts");
    exhausted = vertexPathCount < candidateCount;

    std::vector<CandidatePath> candidates;
    candidates.reserve(vertexPathCount);
    for (igraph_integer_t index = 0; index < vertexPathCount; ++index) {
        const igraph_vector_int_t *vertices = igraph_vector_int_list_get_ptr(vertexPaths.get(), index);
        const igraph_vector_int_t *edgesForPath = igraph_vector_int_list_get_ptr(edgePaths.get(), index);
        if (igraph_vector_int_size(vertices) < 2)
            continue;
        if (igraph_vector_int_size(edgesForPath) + 1 != igraph_vector_int_size(vertices))
            throw std::runtime_error("igraph returned a K-shortest path with inconsistent vertices and edges");
        if (VECTOR(*vertices)[0] != source ||
            VECTOR(*vertices)[igraph_vector_int_size(vertices) - 1] != destination) {
            throw std::runtime_error("igraph returned a K-shortest path with incorrect endpoints");
        }

        CandidatePath candidate;
        candidate.nodeIds.reserve(igraph_vector_int_size(vertices));
        for (igraph_integer_t vertexIndex = 0; vertexIndex < igraph_vector_int_size(vertices); ++vertexIndex)
            candidate.nodeIds.push_back(static_cast<int32_t>(VECTOR(*vertices)[vertexIndex]));
        for (igraph_integer_t edgeIndex = 0; edgeIndex < igraph_vector_int_size(edgesForPath); ++edgeIndex) {
            const int32_t edgeId = static_cast<int32_t>(VECTOR(*edgesForPath)[edgeIndex]);
            if (edgeId < 0 || edgeId >= igraph_vector_size(edgeWeightsMs))
                throw std::runtime_error("igraph returned an invalid edge ID in a K-shortest path");
            candidate.coreOneWayDelayMs += VECTOR(*edgeWeightsMs)[edgeId];
        }
        candidates.push_back(std::move(candidate));
    }
    sortAndDeduplicateCandidates(candidates);
    return candidates;
}

} // namespace

KPathAlgorithm kPathAlgorithmForPolicy(bool edgeDisjoint, int32_t maxSharedCoreLinks)
{
    if (edgeDisjoint)
        return KPathAlgorithm::EdgeDisjointMinCostFlow;
    return maxSharedCoreLinks >= 0 ? KPathAlgorithm::YenOverlapLimited : KPathAlgorithm::Yen;
}

const char *kPathAlgorithmName(KPathAlgorithm algorithm)
{
    switch (algorithm) {
        case KPathAlgorithm::Yen:
            return "yen";
        case KPathAlgorithm::EdgeDisjointMinCostFlow:
            return "edge-disjoint-mincost";
        case KPathAlgorithm::YenOverlapLimited:
            return "yen-overlap-limited";
        default:
            throw std::invalid_argument("Unknown endpoint K-path algorithm");
    }
}

KShortestPathFinder::~KShortestPathFinder()
{
    clear();
}

void KShortestPathFinder::clear()
{
    physicalEdges.clear();
    physicalEdgeWeightsMs.clear();
    if (!initialized)
        return;
    igraph_vector_destroy(&edgeWeightsMs);
    igraph_destroy(&topology);
    initialized = false;
}

void KShortestPathFinder::reset(int32_t nodeCount,
                                const std::vector<std::pair<int32_t, int32_t>>& edges,
                                const std::vector<double>& weightsMs)
{
    if (nodeCount <= 0)
        throw std::invalid_argument("K-shortest path topology must contain at least one node");
    if (edges.size() != weightsMs.size())
        throw std::invalid_argument("K-shortest path edge and weight counts differ");

    std::unordered_set<uint64_t> uniqueEdges;
    uniqueEdges.reserve(edges.size());
    for (size_t index = 0; index < edges.size(); ++index) {
        const auto& [source, destination] = edges[index];
        if (source < 0 || source >= nodeCount || destination < 0 || destination >= nodeCount)
            throw std::invalid_argument("K-shortest path edge endpoint is outside the topology");
        if (source == destination)
            throw std::invalid_argument("K-shortest path topology cannot contain a self-loop");
        if (!uniqueEdges.insert(normalizedEdgeKey(source, destination)).second)
            throw std::invalid_argument("K-shortest path topology cannot contain parallel physical edges");
        if (!std::isfinite(weightsMs[index]) || weightsMs[index] < 0)
            throw std::invalid_argument("K-shortest path edge weight must be finite and non-negative");
    }

    clear();

    igraph_vector_int_t edgeVector;
    igraph_error_t error = igraph_vector_int_init(&edgeVector, edges.size() * 2);
    if (error != IGRAPH_SUCCESS)
        throwIgraphError("igraph_vector_int_init", error);
    for (size_t index = 0; index < edges.size(); ++index) {
        VECTOR(edgeVector)[index * 2] = edges[index].first;
        VECTOR(edgeVector)[index * 2 + 1] = edges[index].second;
    }

    error = igraph_empty(&topology, nodeCount, IGRAPH_UNDIRECTED);
    if (error != IGRAPH_SUCCESS) {
        igraph_vector_int_destroy(&edgeVector);
        throwIgraphError("igraph_empty", error);
    }
    error = igraph_add_edges(&topology, &edgeVector, nullptr);
    igraph_vector_int_destroy(&edgeVector);
    if (error != IGRAPH_SUCCESS) {
        igraph_destroy(&topology);
        throwIgraphError("igraph_add_edges", error);
    }

    error = igraph_vector_init(&edgeWeightsMs, weightsMs.size());
    if (error != IGRAPH_SUCCESS) {
        igraph_destroy(&topology);
        throwIgraphError("igraph_vector_init", error);
    }
    for (size_t index = 0; index < weightsMs.size(); ++index)
        VECTOR(edgeWeightsMs)[index] = weightsMs[index];
    initialized = true;
    try {
        physicalEdges = edges;
        physicalEdgeWeightsMs = weightsMs;
    }
    catch (...) {
        clear();
        throw;
    }
}

int32_t KShortestPathFinder::nodeCount() const
{
    return initialized ? static_cast<int32_t>(igraph_vcount(&topology)) : 0;
}

const igraph_t *KShortestPathFinder::graph() const
{
    if (!initialized)
        throw std::logic_error("K-shortest path topology has not been initialized");
    return &topology;
}

const igraph_vector_t *KShortestPathFinder::weights() const
{
    if (!initialized)
        throw std::logic_error("K-shortest path topology has not been initialized");
    return &edgeWeightsMs;
}

std::vector<KShortestPath> KShortestPathFinder::findPaths(
    int32_t source,
    int32_t destination,
    const KShortestPathOptions& options,
    double sharedAccessOneWayDelayMs) const
{
    if (!initialized)
        throw std::logic_error("K-shortest path topology has not been initialized");
    if (source < 0 || source >= nodeCount() || destination < 0 || destination >= nodeCount())
        throw std::out_of_range("K-shortest path endpoint is outside the topology");
    if (options.pathCount <= 0)
        throw std::invalid_argument("K-shortest path count must be positive");
    if (!std::isfinite(options.maxRttSpreadMs) || options.maxRttSpreadMs < 0)
        throw std::invalid_argument("K-shortest path RTT spread must be finite and non-negative");
    if (options.maxSharedCoreLinks < -1)
        throw std::invalid_argument("K-shortest path shared-link limit must be -1 or non-negative");
    if (options.edgeDisjoint && options.maxSharedCoreLinks != -1)
        throw std::invalid_argument("The shared-link limit applies only to non-edge-disjoint K paths");
    if (!std::isfinite(sharedAccessOneWayDelayMs) || sharedAccessOneWayDelayMs < 0)
        throw std::invalid_argument("Shared access delay must be finite and non-negative");

    if (source == destination)
        return {{{source}, 0, sharedAccessOneWayDelayMs, 2 * sharedAccessOneWayDelayMs}};

    if (options.edgeDisjoint) {
        std::vector<CandidatePath> candidates = findEdgeDisjointCandidates(
            nodeCount(), physicalEdges, physicalEdgeWeightsMs, source, destination, options.pathCount);
        sortAndDeduplicateCandidates(candidates);
        return selectRttRange(candidates, options.pathCount, options.maxRttSpreadMs,
                              sharedAccessOneWayDelayMs);
    }

    int32_t candidateCount = options.pathCount;
    while (true) {
        bool exhausted = false;
        std::vector<CandidatePath> candidates = findYenCandidates(
            &topology, &edgeWeightsMs, source, destination, candidateCount, exhausted);
        std::vector<KShortestPath> selected = selectRttRange(
            candidates, options.pathCount, options.maxRttSpreadMs,
            sharedAccessOneWayDelayMs, options.maxSharedCoreLinks);
        if (options.maxSharedCoreLinks < 0 || selected.size() == static_cast<size_t>(options.pathCount) ||
            exhausted || candidates.empty()) {
            return selected;
        }

        const double lastRttMs = 2 * (candidates.back().coreOneWayDelayMs + sharedAccessOneWayDelayMs);
        const double shortestRttMs = 2 * (candidates.front().coreOneWayDelayMs + sharedAccessOneWayDelayMs);
        if (lastRttMs - shortestRttMs > options.maxRttSpreadMs + DISTANCE_EPSILON_MS)
            return selected;
        if (candidateCount > std::numeric_limits<int32_t>::max() / 2)
            throw std::overflow_error("Overlap-limited K-shortest path search exceeded its candidate range");
        candidateCount *= 2;
    }
}

} // namespace leoRouting
} // namespace inet
