//
// Weighted K-shortest path support for the LEO topology. This component has
// no OMNeT++ dependencies so path selection can be tested independently.
//

#ifndef NETWORKLAYER_CONFIGURATOR_IPV4_LEOKSHORTESTPATHS_H_
#define NETWORKLAYER_CONFIGURATOR_IPV4_LEOKSHORTESTPATHS_H_

#include <cstdint>
#include <utility>
#include <vector>

#include <igraph.h>

namespace inet {
namespace leoRouting {

struct KShortestPathOptions {
    int32_t pathCount = 1;
    double maxRttSpreadMs = 5.0;
    bool edgeDisjoint = false;
};

enum class KPathAlgorithm : int32_t {
    Yen = 1,
    EdgeDisjointMinCostFlow = 2,
};

KPathAlgorithm kPathAlgorithmForPolicy(bool edgeDisjoint);
const char *kPathAlgorithmName(KPathAlgorithm algorithm);

struct KShortestPath {
    std::vector<int32_t> nodeIds;
    double coreOneWayDelayMs = 0;
    double oneWayDelayMs = 0;
    double rttMs = 0;
};

struct KShortestPathGroup {
    int32_t sourceNodeId = -1;
    int32_t destinationNodeId = -1;
    int32_t coreSourceNodeId = -1;
    int32_t coreDestinationNodeId = -1;
    uint64_t topologyGeneration = 0;
    uint64_t endpointAttachmentGeneration = 0;
    int32_t requestedPathCount = 0;
    double maxRttSpreadMs = 0;
    bool edgeDisjoint = false;
    std::vector<KShortestPath> paths;
};

class KShortestPathFinder {
  private:
    igraph_t topology;
    igraph_vector_t edgeWeightsMs;
    std::vector<std::pair<int32_t, int32_t>> physicalEdges;
    std::vector<double> physicalEdgeWeightsMs;
    bool initialized = false;

    void clear();

  public:
    KShortestPathFinder() = default;
    ~KShortestPathFinder();

    KShortestPathFinder(const KShortestPathFinder&) = delete;
    KShortestPathFinder& operator=(const KShortestPathFinder&) = delete;

    void reset(int32_t nodeCount,
               const std::vector<std::pair<int32_t, int32_t>>& edges,
               const std::vector<double>& weightsMs);

    bool isInitialized() const { return initialized; }
    int32_t nodeCount() const;
    const igraph_t *graph() const;
    const igraph_vector_t *weights() const;

    std::vector<KShortestPath> findPaths(int32_t source,
                                        int32_t destination,
                                        const KShortestPathOptions& options,
                                        double sharedAccessOneWayDelayMs = 0) const;
};

} // namespace leoRouting
} // namespace inet

#endif
