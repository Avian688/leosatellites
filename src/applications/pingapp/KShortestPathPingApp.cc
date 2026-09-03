//
// Ping application that selects a saved endpoint K-shortest path.
//

#include "KShortestPathPingApp.h"

#include <inet/common/ModuleAccess.h>

#include "KShortestPathPingIdentifier.h"
#include "../../networklayer/configurator/ipv4/LeoIpv4NetworkConfigurator.h"

namespace inet {

Define_Module(KShortestPathPingApp);

simsignal_t KShortestPathPingApp::pathAvailableSignal = registerSignal("kPathAvailable");
simsignal_t KShortestPathPingApp::expectedRttSignal = registerSignal("kPathExpectedRtt");
simsignal_t KShortestPathPingApp::coreLinkCountSignal = registerSignal("kPathCoreLinkCount");
simsignal_t KShortestPathPingApp::catalogSizeSignal = registerSignal("kPathCatalogSize");

void KShortestPathPingApp::initialize(int stage)
{
    PingApp::initialize(stage);
    if (stage != INITSTAGE_LOCAL)
        return;

    pathGroup = par("pathGroup");
    pathIndex = par("pathIndex");
    if (pathGroup < 0 || pathGroup >= K_PATH_PING_MAX_GROUPS)
        throw cRuntimeError("pathGroup must be in the range 0..%d", K_PATH_PING_MAX_GROUPS - 1);
    if (pathIndex < 1 || pathIndex > K_PATH_PING_MAX_PATHS)
        throw cRuntimeError("pathIndex must be in the range 1..%d", K_PATH_PING_MAX_PATHS);

    cModule *host = getContainingNode(this);
    cModule *network = host != nullptr ? host->getParentModule() : nullptr;
    configurator = network != nullptr ?
        dynamic_cast<LeoIpv4NetworkConfigurator *>(network->getSubmodule("configurator")) : nullptr;
    if (configurator == nullptr)
        throw cRuntimeError("KShortestPathPingApp requires a sibling LeoIpv4NetworkConfigurator");
}

void KShortestPathPingApp::startSendingPingRequests()
{
    PingApp::startSendingPingRequests();
    pid = makeKPathPingIdentifier(pathGroup, pathIndex);
}

void KShortestPathPingApp::emitPathState()
{
    cModule *host = getContainingNode(this);
    const int sourceNodeId = configurator->getNodeModuleGraphId(host->getFullName());
    const int destinationNodeId = configurator->getKPathPingPeerNodeId(pathGroup, sourceNodeId);
    if (sourceNodeId < 0 || destinationNodeId < 0)
        throw cRuntimeError("Cannot resolve K-path ping endpoints for %s", getFullPath().c_str());

    leoRouting::KShortestPathGroup group;
    const bool hasCurrentGroup = configurator->tryGetKPathPingPathGroup(
        pathGroup, sourceNodeId, destinationNodeId, -1, group);
    const bool available = hasCurrentGroup && group.paths.size() >= static_cast<size_t>(pathIndex);
    emit(pathAvailableSignal, available ? 1L : 0L);
    emit(catalogSizeSignal, hasCurrentGroup ? static_cast<long>(group.paths.size()) : 0L);
    if (available) {
        const leoRouting::KShortestPath& path = group.paths[pathIndex - 1];
        emit(expectedRttSignal, path.rttMs / 1000.0);
        const long coreLinkCount = path.nodeIds.size() >= 3 ?
            static_cast<long>(path.nodeIds.size() - 3) : 0L;
        emit(coreLinkCountSignal, coreLinkCount);
    }
    else {
        emit(expectedRttSignal, -1.0);
        emit(coreLinkCountSignal, -1L);
    }
}

void KShortestPathPingApp::sendPingRequest()
{
    emitPathState();
    PingApp::sendPingRequest();
}

} // namespace inet
