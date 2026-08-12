//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 
#include<fstream>
#include <sstream>
#include<iostream>
#include <filesystem>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include "LeoIpv4NetworkConfigurator.h"

namespace inet {
Define_Module(LeoIpv4NetworkConfigurator);

static void silent_warning_handler(const char *reason, const char *file, int line) {
    // Silence all warnings as the console will be filled with warnings if a ground station cannot connect to a satellite
}

static std::string trimWhitespace(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
        begin++;
    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
        end--;
    return value.substr(begin, end - begin);
}

LeoIpv4NetworkConfigurator::~LeoIpv4NetworkConfigurator()
{
    nodeModules.clear();
    ipv4Modules.clear();
    igraph_vector_int_destroy(&islVec);
}

void LeoIpv4NetworkConfigurator::finish()
{
    if (!logRouteSnapshotStats)
        return;
    std::cout << "LEO_ROUTE_STATS"
              << " filesRead=" << routeSnapshotFilesRead
              << " bytesRead=" << routeSnapshotBytesRead
              << " recordsDecoded=" << routeRecordsDecoded
              << " operationsApplied=" << routeOperationsApplied
              << " sourceRowsRebuilt=" << routeSourceRowsRebuilt
              << " entriesResolved=" << routeEntriesResolved
              << std::endl;
    std::cout << "LEO_KPATH_STATS"
              << " filesRead=" << kPathSnapshotFilesRead
              << " bytesRead=" << kPathSnapshotBytesRead
              << " groupsLoaded=" << kPathGroupsLoaded
              << " pathsLoaded=" << kPathsLoaded
              << " nodeIdsLoaded=" << kPathNodeIdsLoaded
              << std::endl;
}

void LeoIpv4NetworkConfigurator::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        // Disable igraph warnings
        igraph_set_warning_handler(&silent_warning_handler);

        // Get reference to the parent module
        cModule *parent = getParentModule();

        // Read module and configuration parameters
        networkName = parent->getName();
        loadFiles = par("loadFiles");
        allowRouteSnapshotOverwrite = par("allowRouteSnapshotOverwrite");
        logRouteSnapshotStats = par("logRouteSnapshotStats");
        minLinkWeight = par("minLinkWeight");
        configureIsolatedNetworksSeparatly = par("configureIsolatedNetworksSeparatly").boolValue();

        // Read satellite network parameters from parent
        numOfSats = parent->par("numOfSats");
        numOfGS = parent->par("numOfGS");
        numOfClients = parent->par("numOfClients");
        numOfUserTerminals = parent->par("numOfUserTerminals");
        satPerPlane = parent->par("satsPerPlane");
        configLocation = par("configLocation").stringValue();
        unsigned int planes = parent->par("numOfPlanes");

        // Calculate total number of planes (ensures all satellites are distributed)
        numOfPlanes = (int)std::ceil(((double)numOfSats / ((double)planes * (double)satPerPlane)) * (double)planes);

        // Set number of inter-satellite links (2 per satellite in a grid-like pattern)
        numOfISLs = numOfSats * 2;

        // Assign unique IDs to modules (custom method)
        assignIDtoModules();
        ipv4Modules.resize(nodeModules.size(), nullptr);
        const int routableNodeCount = std::min(static_cast<int>(numOfSats + numOfGS), static_cast<int>(nodeModules.size()));
        nextHopInterfaces.reset(routableNodeCount, nodeModules.size());

        // Path calculation parameters
        numOfKPaths = par("numOfKPaths");
        kPathsEdgeDisjoint = par("kPathsEdgeDisjoint");
        kPathMaxRttSpreadMs = par("kPathMaxRttSpread").doubleValueInUnit("ms");
        kPathSnapshotSet = par("kPathSnapshotSet").stringValue();
        kPathEndpointPairsSpec = par("kPathEndpointPairs").stringValue();
        if (numOfKPaths < 1)
            throw cRuntimeError("numOfKPaths must be at least 1");
        if (!std::isfinite(kPathMaxRttSpreadMs) || kPathMaxRttSpreadMs < 0)
            throw cRuntimeError("kPathMaxRttSpread must be finite and non-negative");
        currentInterval = 0;

        // Read orbit characteristics
        double altitude = parent->par("alt");
        double inclination = parent->par("incl");

        // Convert altitude and inclination to strings
        std::string altitudeStr = std::to_string(altitude);
        std::string inclinationStr = std::to_string(inclination);

        // Trim trailing zeros and a possible trailing dot from altitude
        altitudeStr.erase(altitudeStr.find_last_not_of('0') + 1);
        if (!altitudeStr.empty() && altitudeStr.back() == '.') {
            altitudeStr.pop_back();
        }

        // Trim trailing zeros and a possible trailing dot from inclination
        inclinationStr.erase(inclinationStr.find_last_not_of('0') + 1);
        if (!inclinationStr.empty() && inclinationStr.back() == '.') {
            inclinationStr.pop_back();
        }

        // Determine the topology tag based on ISL setting
        std::string topologyTag = parent->par("enableInterSatelliteLinks").boolValue() ? "_ISL" : "_BP";

        // Compose the file prefix using underscores as separators
        filePrefix = std::to_string(numOfSats) + "_" + altitudeStr + "_" + std::to_string(planes) + "_" + std::to_string(satPerPlane) + "_" + inclinationStr + "_" + std::to_string(numOfGS) + topologyTag;

        // Saving no longer removes an existing corpus. Each snapshot is
        // published atomically, and overwrite requires an explicit opt-in.
        writeModuleIDMappingsToFile((getRoutingDirectory() / "idMap.txt").string());

        updateModuleIDMappingsClientServer();
        initializeKPathSnapshotConfiguration();

        igraph_vector_int_init(&islVec, 0);
    }
}


std::filesystem::path LeoIpv4NetworkConfigurator::getRoutingDirectory() const
{
    const std::filesystem::path root(configLocation);
    return root.empty() ? std::filesystem::path(filePrefix) : root / filePrefix;
}

std::filesystem::path LeoIpv4NetworkConfigurator::getKPathSnapshotDirectory() const
{
    const std::string profile = leoRouting::makeKPathSnapshotProfileName(
        leoRouting::kPathAlgorithmForPolicy(kPathsEdgeDisjoint), numOfKPaths,
        kPathMaxRttSpreadMs, configuredKPathEndpointPairs);
    return getRoutingDirectory() / "kpaths" / kPathSnapshotSet / profile;
}

std::filesystem::path LeoIpv4NetworkConfigurator::getKPathSnapshotPath(simtime_t interval) const
{
    return getKPathSnapshotDirectory() / (interval.str() + ".bin");
}

void LeoIpv4NetworkConfigurator::initializeKPathSnapshotConfiguration()
{
    const std::string mode = trimWhitespace(par("kPathSnapshotMode").stringValue());
    if (mode == "disabled")
        kPathSnapshotMode = KPathSnapshotMode::Disabled;
    else if (mode == "generate")
        kPathSnapshotMode = KPathSnapshotMode::Generate;
    else if (mode == "load")
        kPathSnapshotMode = KPathSnapshotMode::Load;
    else
        throw cRuntimeError("Unknown kPathSnapshotMode '%s'; expected disabled, generate, or load",
                            mode.c_str());

    if (kPathSnapshotMode == KPathSnapshotMode::Disabled)
        return;
    if (numOfKPaths <= 1)
        throw cRuntimeError("kPathSnapshotMode=%s requires numOfKPaths > 1", mode.c_str());
    if (kPathSnapshotMode == KPathSnapshotMode::Load && !loadFiles)
        throw cRuntimeError("kPathSnapshotMode=load requires loadFiles=true");

    kPathSnapshotSet = trimWhitespace(kPathSnapshotSet);
    if (kPathSnapshotSet.empty())
        throw cRuntimeError("An explicit kPathSnapshotSet is required for endpoint-specific K paths");
    if (kPathSnapshotSet == "." || kPathSnapshotSet == ".." ||
        !std::all_of(kPathSnapshotSet.begin(), kPathSnapshotSet.end(), [](unsigned char character) {
            return std::isalnum(character) || character == '_' || character == '-' || character == '.';
        })) {
        throw cRuntimeError("kPathSnapshotSet '%s' must be one safe directory-name component",
                            kPathSnapshotSet.c_str());
    }
    initializeKPathEndpointPairs();
}

void LeoIpv4NetworkConfigurator::initializeKPathEndpointPairs()
{
    configuredKPathEndpointPairs.clear();
    const int32_t endpointStart = std::min(static_cast<int32_t>(numOfSats + numOfGS),
                                           static_cast<int32_t>(nodeModules.size()));
    const std::string specification = trimWhitespace(kPathEndpointPairsSpec);
    if (specification == "all") {
        std::vector<int32_t> endpoints;
        for (int32_t nodeId = endpointStart; nodeId < static_cast<int32_t>(nodeModules.size()); ++nodeId) {
            if (nodeModules[nodeId] != nullptr)
                endpoints.push_back(nodeId);
        }
        for (size_t first = 0; first < endpoints.size(); ++first) {
            for (size_t second = first + 1; second < endpoints.size(); ++second)
                configuredKPathEndpointPairs.emplace_back(endpoints[first], endpoints[second]);
        }
    }
    else {
        std::istringstream pairs(specification);
        std::string token;
        while (std::getline(pairs, token, ',')) {
            token = trimWhitespace(token);
            const size_t delimiter = token.find("->");
            if (delimiter == std::string::npos || token.find("->", delimiter + 2) != std::string::npos)
                throw cRuntimeError("Invalid kPathEndpointPairs entry '%s'; expected endpointA->endpointB",
                                    token.c_str());
            const std::string sourceName = trimWhitespace(token.substr(0, delimiter));
            const std::string destinationName = trimWhitespace(token.substr(delimiter + 2));
            auto source = moduleGraphIDMap.find(sourceName);
            auto destination = moduleGraphIDMap.find(destinationName);
            if (source == moduleGraphIDMap.end() || destination == moduleGraphIDMap.end())
                throw cRuntimeError("Unknown endpoint in kPathEndpointPairs entry '%s'", token.c_str());
            if (source->second < endpointStart || destination->second < endpointStart)
                throw cRuntimeError("kPathEndpointPairs entry '%s' must contain end hosts, not core nodes",
                                    token.c_str());
            if (nodeModules[source->second] == nullptr || nodeModules[destination->second] == nullptr)
                throw cRuntimeError("kPathEndpointPairs entry '%s' refers to a missing endpoint module",
                                    token.c_str());
            configuredKPathEndpointPairs.push_back(
                leoRouting::normalizeKPathEndpointPair(source->second, destination->second));
        }
    }

    if (configuredKPathEndpointPairs.empty())
        throw cRuntimeError("No end-to-end pairs were selected by kPathEndpointPairs='%s'",
                            specification.c_str());
    std::sort(configuredKPathEndpointPairs.begin(), configuredKPathEndpointPairs.end());
    for (size_t index = 1; index < configuredKPathEndpointPairs.size(); ++index) {
        if (configuredKPathEndpointPairs[index - 1] == configuredKPathEndpointPairs[index])
            throw cRuntimeError("kPathEndpointPairs contains a duplicate endpoint pair");
    }
}

LeoIpv4 *LeoIpv4NetworkConfigurator::getIpv4Module(int nodeId)
{
    if (nodeId < 0 || nodeId >= static_cast<int>(ipv4Modules.size()))
        return nullptr;
    LeoIpv4 *ipv4Mod = ipv4Modules[nodeId];
    if (ipv4Mod == nullptr) {
        cModule *nodeMod = nodeModules[nodeId];
        ipv4Mod = nodeMod != nullptr ? dynamic_cast<LeoIpv4 *>(nodeMod->getModuleByPath(".ipv4.ip")) : nullptr;
        ipv4Modules[nodeId] = ipv4Mod;
    }
    return ipv4Mod;
}

void LeoIpv4NetworkConfigurator::applyFullRouteState(leoRouting::StableRouteState&& candidateState)
{
    std::vector<std::vector<int>> resolvedRows(candidateState.sourceCount());
    for (int source = 0; source < candidateState.sourceCount(); ++source)
        resolvedRows[source] = leoRouting::resolveSourceRow(candidateState, nextHopInterfaces, source, nodeModules.size());

    // All records and interfaces have been validated before forwarding changes.
    for (int source = 0; source < candidateState.sourceCount(); ++source) {
        LeoIpv4 *ipv4Mod = getIpv4Module(source);
        if (ipv4Mod != nullptr)
            ipv4Mod->replacePrimaryNextHopInterfaces(resolvedRows[source]);
    }

    routeOperationsApplied += candidateState.routeCount();
    routeSourceRowsRebuilt += candidateState.sourceCount();
    routeEntriesResolved += candidateState.routeCount();
    stableRouteState = std::move(candidateState);
    nextHopInterfaces.clearAllDirty();
}

void LeoIpv4NetworkConfigurator::applyDeltaRouteState(const leoRouting::ParsedSnapshot& snapshot)
{
    const leoRouting::DeltaPreview preview = stableRouteState.validateDelta(snapshot);

    // The complete delta is structurally and semantically valid at this point.
    // Updating stable state is still separate from the live forwarding commit.
    stableRouteState.applyValidatedDelta(snapshot, preview);

    const std::vector<int32_t> dirtySources = nextHopInterfaces.dirtySources();
    std::vector<uint8_t> rebuildSource(stableRouteState.sourceCount(), 0);
    std::vector<std::pair<int32_t, std::vector<int>>> resolvedRows;
    resolvedRows.reserve(dirtySources.size());
    for (int32_t source : dirtySources) {
        rebuildSource[source] = 1;
        resolvedRows.emplace_back(source,
            leoRouting::resolveSourceRow(stableRouteState, nextHopInterfaces, source, nodeModules.size()));
        const auto rowBegin = stableRouteState.rawRoutes().begin() +
                              static_cast<size_t>(source) * stableRouteState.destinationCount();
        routeEntriesResolved += std::count_if(rowBegin, rowBegin + stableRouteState.destinationCount(),
                                              [](int32_t value) { return value != leoRouting::DELETE_NEXT_HOP; });
    }

    struct ResolvedChange {
        int32_t source;
        int32_t destination;
        int interfaceId;
    };
    std::vector<ResolvedChange> resolvedChanges;
    resolvedChanges.reserve(snapshot.records.size());
    for (const leoRouting::RouteRecord& record : snapshot.records) {
        if (rebuildSource[record.source])
            continue;
        const int interfaceId = leoRouting::resolveRoute(stableRouteState, nextHopInterfaces,
                                                         record.source, record.destination);
        resolvedChanges.push_back({record.source, record.destination, interfaceId});
        if (record.nextHop != leoRouting::DELETE_NEXT_HOP)
            routeEntriesResolved++;
    }

    // Resolution above may throw, so no live row is modified before every
    // affected route has a valid current interface.
    for (auto& [source, row] : resolvedRows) {
        LeoIpv4 *ipv4Mod = getIpv4Module(source);
        if (ipv4Mod != nullptr)
            ipv4Mod->replacePrimaryNextHopInterfaces(row);
        nextHopInterfaces.clearDirty(source);
    }
    for (const ResolvedChange& change : resolvedChanges) {
        LeoIpv4 *ipv4Mod = getIpv4Module(change.source);
        if (ipv4Mod != nullptr)
            ipv4Mod->setPrimaryNextHopInterface(change.destination, change.interfaceId);
    }

    routeOperationsApplied += snapshot.records.size();
    routeSourceRowsRebuilt += resolvedRows.size();
}

bool LeoIpv4NetworkConfigurator::loadConfiguration(simtime_t currentInterval)
{
    const std::filesystem::path filePath = getRoutingDirectory() / (currentInterval.str() + ".bin");
    if (!std::filesystem::exists(filePath)) {
        if (stableRouteState.hasSequenceMetadata())
            throw cRuntimeError("Missing routing delta after sequence %d: %s",
                                stableRouteState.sequence(), filePath.string().c_str());
        return false;
    }

    try {
        const leoRouting::ParsedSnapshot snapshot = leoRouting::readSnapshot(filePath);
        routeSnapshotFilesRead++;
        routeSnapshotBytesRead += snapshot.bytesRead;
        routeRecordsDecoded += snapshot.records.size();

        const int routableNodeCount = std::min(static_cast<int>(numOfSats + numOfGS),
                                               static_cast<int>(nodeModules.size()));
        if (snapshot.format == leoRouting::SnapshotFormat::V3 &&
            snapshot.header.kind == leoRouting::SnapshotKind::Base &&
            stableRouteState.hasSequenceMetadata()) {
            throw std::runtime_error("Unexpected LEO3 base snapshot after loaded sequence " +
                                     std::to_string(stableRouteState.sequence()));
        }
        if (snapshot.format == leoRouting::SnapshotFormat::V2Full ||
            snapshot.header.kind == leoRouting::SnapshotKind::Base) {
            leoRouting::FullDecodeStats decodeStats;
            leoRouting::StableRouteState candidate = leoRouting::StableRouteState::fromFullSnapshot(
                snapshot, routableNodeCount, routableNodeCount, &decodeStats);
            applyFullRouteState(std::move(candidate));
        }
        else {
            applyDeltaRouteState(snapshot);
        }
        return true;
    }
    catch (const std::exception& error) {
        throw cRuntimeError("Failed to load routing snapshot %s: %s",
                            filePath.string().c_str(), error.what());
    }
}

void LeoIpv4NetworkConfigurator::assignIDtoModules()
{
    const int totalNodes = numOfSats + numOfGS + (numOfClients * 2) + numOfUserTerminals;
    nodeModules.resize(totalNodes, nullptr);
    for(int nodeNum = 0; nodeNum < totalNodes; nodeNum++){
        nodeModules[nodeNum] = getNodeModule(nodeNum);
        if (nodeModules[nodeNum] != nullptr)
            moduleGraphIdByModule[nodeModules[nodeNum]] = nodeNum;
    }
}

void LeoIpv4NetworkConfigurator::updateForwardingStates(simtime_t currentInterval)
{
    if(loadFiles){
        bool completedLoad = loadConfiguration(currentInterval);
        if(!completedLoad){
            std::cerr << "WARNING: Failed to load routing files at simtime: " << simTime() << " Make sure you generate routing files before using loadFile = false in ini file." << endl;
        }
        else if (kPathSnapshotMode == KPathSnapshotMode::Generate) {
            rebuildCurrentPathTopology();
            generateKPathSnapshot(currentInterval, stableRouteState);
        }
        else if (kPathSnapshotMode == KPathSnapshotMode::Load) {
            loadKPathSnapshot(currentInterval, stableRouteState);
        }
    }
    else{
        generateTopologyGraph(currentInterval);
        if (kPathSnapshotMode == KPathSnapshotMode::Generate)
            generateKPathSnapshot(currentInterval, generatedRouteState);
    }

}

void LeoIpv4NetworkConfigurator::clearGroundStationLinks()
{
    while (!groundStationLinks.empty())
        groundStationLinks.pop();
}

void LeoIpv4NetworkConfigurator::establishInitialISLs()
{
    unsigned int islVecIterator = 0;
    for(int planeNum = 0; planeNum < numOfPlanes; planeNum++){
        unsigned int numOfSatsInPlane =  planeNum*satPerPlane+satPerPlane;
        if(numOfSats < numOfSatsInPlane){
            numOfSatsInPlane = numOfSats;
        }
        for(unsigned int satNum = planeNum*satPerPlane; satNum < numOfSatsInPlane; satNum++){
            cModule *satMod = nodeModules[satNum]; // get source satellite module
            SatelliteMobility* sourceSatMobility = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"));
            int destSatNumA = (satNum+1)%(satPerPlane*(planeNum+1));
            if(destSatNumA == 0){
                destSatNumA = planeNum*satPerPlane; //If number is zero, must be start of orbital plane
            }
            if(destSatNumA < numOfSats){
                cModule *destModA = nodeModules[destSatNumA];
                //VECTOR(islVec)[islVecIterator] = satNum; VECTOR(islVec)[islVecIterator+1] = destSatNumA;
                igraph_vector_int_push_back(&islVec, satNum);
                igraph_vector_int_push_back(&islVec, destSatNumA);

                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModA->getModuleByPath(".mobility"));
                satelliteISLMobilityEdges.emplace_back(sourceSatMobility, destSatMobility);

                islVecIterator = islVecIterator + 2;
//                for(int i = 0; i < satMod->gateSize("pppg$o"); i++){  //check each possible pppg gate
//                    cGate* srcGate = satMod->gate("pppg$o", i);
//                    if(srcGate->isConnected()){
//                        cChannel *chan = srcGate->getChannel();
//                        cModule* destModule = srcGate->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
//                        //std::cout << "\n" << destModA->getFullName() << " - " << destModule->getFullName() << endl;
//                        if(destModA == destModule){
//                            std::string mobilityName = destModA->getModuleByPath(".mobility")->getNedTypeName();
//                            double distance = 0;
//                            if(mobilityName == "leosatellites.mobility.SatelliteMobility"){
//                                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModA->getModuleByPath(".mobility"));
//                            }
//                        }
//                    }
//                }
            }
            int destSatNumB = (satNum + satPerPlane);// % totalSats;
            if(destSatNumB < numOfSats){
                cModule *destModB = nodeModules[destSatNumB];
                //VECTOR(islVec)[islVecIterator] = satNum; VECTOR(islVec)[islVecIterator+1] = destSatNumB;
                igraph_vector_int_push_back(&islVec, satNum);
                igraph_vector_int_push_back(&islVec, destSatNumB);

                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModB->getModuleByPath(".mobility"));
                satelliteISLMobilityEdges.emplace_back(sourceSatMobility, destSatMobility);

                //islVecIterator = islVecIterator + 2;
//                for(int i = 0; i < satMod->gateSize("pppg$o"); i++){  //check each possible pppg gate
//                    cGate* srcGate = satMod->gate("pppg$o", i);
//                    if(srcGate->isConnected()){
//                        cChannel *chan = srcGate->getChannel();
//                        cModule* destModule = srcGate->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
//                        if(destModB == destModule){
//                            std::string mobilityName = destModB->getModuleByPath(".mobility")->getNedTypeName();
//                            double distance = 0;
//                            if(mobilityName == "leosatellites.mobility.SatelliteMobility"){
//                                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModB->getModuleByPath(".mobility"));
//                            }
//                        }
//                    }
//                }
            }
        }
    }
}

void LeoIpv4NetworkConfigurator::rebuildCurrentPathTopology()
{
    const int routableNodeCount = std::min(static_cast<int>(numOfSats + numOfGS),
                                           static_cast<int>(nodeModules.size()));
    std::vector<std::pair<int32_t, int32_t>> edges;
    std::vector<double> weightsMs;
    edges.reserve(igraph_vector_int_size(&islVec) / 2 + groundStationLinks.size());
    weightsMs.reserve(edges.capacity());

    for (igraph_integer_t index = 0; index < igraph_vector_int_size(&islVec); index += 2) {
        edges.emplace_back(static_cast<int32_t>(VECTOR(islVec)[index]),
                           static_cast<int32_t>(VECTOR(islVec)[index + 1]));
    }

    for (const auto& [sourceMobility, destinationMobility] : satelliteISLMobilityEdges) {
        const double distanceMeters = sourceMobility->getDistance(destinationMobility->getLatitude(),
                                                                   destinationMobility->getLongitude(),
                                                                   destinationMobility->getAltitude()) * 1000;
        weightsMs.push_back((distanceMeters / 299792458.0) * 1000);
    }
    if (edges.size() != weightsMs.size())
        throw cRuntimeError("ISL edge/weight count mismatch while building the path topology: %zu edges, %zu weights",
                            edges.size(), weightsMs.size());

    std::queue<std::tuple<int, int, double>> currentGroundStationLinks = groundStationLinks;
    while (!currentGroundStationLinks.empty()) {
        const auto [source, destination, weightMs] = currentGroundStationLinks.front();
        currentGroundStationLinks.pop();
        edges.emplace_back(source, destination);
        weightsMs.push_back(weightMs);
    }

    currentPathTopology.reset(routableNodeCount, edges, weightsMs);
    pathTopologyGeneration++;
}

void LeoIpv4NetworkConfigurator::generateTopologyGraph(simtime_t currentInterval)
{
    const int routableNodeCount = std::min(static_cast<int>(numOfSats + numOfGS),
                                           static_cast<int>(nodeModules.size()));
    leoRouting::StableRouteState currentRouteState(routableNodeCount, routableNodeCount);
    rebuildCurrentPathTopology();

    for (int nodeNum = 0; nodeNum < numOfSats+numOfGS; nodeNum++) {
        LeoIpv4 *ipv4Mod = ipv4Modules[nodeNum];
        if (ipv4Mod == nullptr) {
            cModule* mod = nodeModules[nodeNum];
            ipv4Mod = dynamic_cast<LeoIpv4 *>(mod->getModuleByPath(".ipv4.ip"));
            ipv4Modules[nodeNum] = ipv4Mod;
        }
        if (ipv4Mod != nullptr)
            ipv4Mod->clearNextHops();
    }
    igraph_vector_int_t shortestPathVertexVec;
    igraph_vector_int_init(&shortestPathVertexVec, 0);
    igraph_vector_int_list_t vertexPaths;
    igraph_vector_int_list_init(&vertexPaths, 0);

    for (int sourceNodeNum = 0; sourceNodeNum < numOfSats + numOfGS; sourceNodeNum++) {
        const igraph_error_t error = igraph_get_all_shortest_paths_dijkstra(
            currentPathTopology.graph(), &vertexPaths, nullptr,
            &shortestPathVertexVec, sourceNodeNum, igraph_vss_all(),
            currentPathTopology.weights(), IGRAPH_ALL);
        if (error != IGRAPH_SUCCESS)
            throw cRuntimeError("igraph_get_all_shortest_paths_dijkstra failed for source %d: %s",
                                sourceNodeNum, igraph_strerror(error));
        cModule *sourceMod = nodeModules[sourceNodeNum];
        LeoIpv4 *srcIpv4Mod = ipv4Modules[sourceNodeNum];
        if (srcIpv4Mod == nullptr) {
            srcIpv4Mod = dynamic_cast<LeoIpv4 *>(sourceMod->getModuleByPath(".ipv4.ip"));
            ipv4Modules[sourceNodeNum] = srcIpv4Mod;
        }
        for (igraph_integer_t i = 0; i < igraph_vector_int_list_size(&vertexPaths); i++) {
            const igraph_vector_int_t *path = igraph_vector_int_list_get_ptr(&vertexPaths, i);
            if (igraph_vector_int_size(path) < 2)
                continue;
            int pathSourceNodeNum = igraph_vector_int_get(path, 0);
            int nextHopNodeNum = igraph_vector_int_get(path, 1);
            int destinationNodeNum = igraph_vector_int_get(path, igraph_vector_int_size(path) - 1);
            if (pathSourceNodeNum != destinationNodeNum) {
                int nextHopID = nextHopInterfaces.get(pathSourceNodeNum, nextHopNodeNum);
                srcIpv4Mod->addKNextHop(1, destinationNodeNum, nextHopID);
                currentRouteState.setRoute(pathSourceNodeNum, destinationNodeNum, nextHopNodeNum);
            }
        }
        igraph_vector_int_list_clear(&vertexPaths);
    }
    igraph_vector_int_list_destroy(&vertexPaths);
    igraph_vector_int_destroy(&shortestPathVertexVec);
    writeGeneratedRouteSnapshot(currentRouteState, currentInterval);
}

void LeoIpv4NetworkConfigurator::writeGeneratedRouteSnapshot(
    const leoRouting::StableRouteState& currentState, simtime_t currentInterval)
{
    const int64_t timestampMicros = leoRouting::parseTimestampMicros(currentInterval.str());
    const std::filesystem::path filePath = getRoutingDirectory() / (currentInterval.str() + ".bin");
    leoRouting::StableRouteState stateWithMetadata = currentState;
    leoRouting::SnapshotHeader header;
    std::vector<leoRouting::RouteRecord> records;

    if (!hasGeneratedRouteSnapshot) {
        header = leoRouting::makeBaseHeader(currentState, 0, timestampMicros);
        records = currentState.effectiveRecords();
    }
    else {
        const int32_t sequence = generatedRouteState.sequence() + 1;
        header = leoRouting::makeDeltaHeader(generatedRouteState, currentState, sequence, timestampMicros);
        records = leoRouting::diffRoutes(generatedRouteState, currentState);
    }

    leoRouting::writeV3SnapshotAtomic(filePath, header, records, allowRouteSnapshotOverwrite);
    stateWithMetadata.setSnapshotMetadata(header);
    generatedRouteState = std::move(stateWithMetadata);
    hasGeneratedRouteSnapshot = true;
}

void LeoIpv4NetworkConfigurator::addNextHopInterface(cModule* source, cModule* destination, int interfaceID)
{
    auto sourceIt = moduleGraphIdByModule.find(source);
    auto destinationIt = moduleGraphIdByModule.find(destination);
    if (sourceIt == moduleGraphIdByModule.end() || destinationIt == moduleGraphIdByModule.end())
        return;
    nextHopInterfaces.set(sourceIt->second, destinationIt->second, interfaceID);
}

void LeoIpv4NetworkConfigurator::removeNextHopInterface(cModule* source, cModule* destination)
{
    auto sourceIt = moduleGraphIdByModule.find(source);
    auto destinationIt = moduleGraphIdByModule.find(destination);
    if (sourceIt == moduleGraphIdByModule.end() || destinationIt == moduleGraphIdByModule.end())
        return;
    nextHopInterfaces.set(sourceIt->second, destinationIt->second, -1);
}

void LeoIpv4NetworkConfigurator::addGSLinktoTopologyGraph(int sourceNum, int destNum, double weight)
{
    groundStationLinks.push(std::tuple<int, int, double>(sourceNum, destNum, weight));
}

cModule* LeoIpv4NetworkConfigurator::getNodeModule(int nodeNumber)
{
    cModule* mod;
    if(nodeNumber < numOfSats){
        std::string nodeName = std::string(networkName + ".satellite[" + std::to_string(nodeNumber) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    else if(nodeNumber >= numOfSats && nodeNumber < numOfSats+numOfGS){
        std::string nodeName = std::string(networkName + ".groundStation[" + std::to_string(nodeNumber-numOfSats) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    else if(nodeNumber >= numOfSats+numOfGS && nodeNumber < numOfSats+numOfGS+numOfClients){
        std::string nodeName = std::string(networkName + ".client[" + std::to_string(nodeNumber-(numOfSats+numOfGS)) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    else if (nodeNumber >= numOfSats + numOfGS + numOfClients && nodeNumber < numOfSats + numOfGS + (numOfClients * 2)) {
        std::string nodeName = std::string(networkName + ".server[" + std::to_string(nodeNumber-(numOfSats+numOfGS+numOfClients)) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    else {
        std::string nodeName = std::string(networkName + ".userTerminal[" + std::to_string(nodeNumber-(numOfSats+numOfGS+(numOfClients*2))) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    return mod;
}

cModule* LeoIpv4NetworkConfigurator::getClientServerModule(bool client, int nodeNumber)
{
    cModule* mod;
    if(client){
        std::string nodeName = std::string(networkName + ".client[" + std::to_string(nodeNumber) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    else{
        std::string nodeName = std::string(networkName + ".server[" + std::to_string(nodeNumber) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    return mod;
}

#include <fstream>
#include <filesystem>
#include <map>

void LeoIpv4NetworkConfigurator::writeModuleIDMappingsToFile(const std::string& filePath)
{
    namespace fs = std::filesystem;

    std::ofstream outFile;
    fs::path temporaryPath;

    if (!loadFiles) {
        fs::path pathObj(filePath);
        fs::path dir = pathObj.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            if (!fs::create_directories(dir)) {
                std::cout << "Error: Could not create directory " << dir << "\n";
                return;
            }
        }
        if (!allowRouteSnapshotOverwrite && fs::exists(pathObj))
            throw cRuntimeError("Refusing to overwrite existing route ID map: %s", filePath.c_str());
        temporaryPath = pathObj.string() + ".tmp";
        outFile.open(temporaryPath, std::ios::trunc);
        if (!outFile.is_open()) {
            EV << "Error: Could not open temporary file " << temporaryPath << " for writing.\n";
            return;
        }
    }

    // Write module mappings and populate the dictionary
    for (int nodeNum = 0; nodeNum < numOfSats + numOfGS; ++nodeNum) {
        std::string nodeName;
        if (nodeNum < numOfSats) {
            nodeName = "satellite[" + std::to_string(nodeNum) + "]";
        } else {
            nodeName = "groundStation[" + std::to_string(nodeNum - numOfSats) + "]";
        }

        int moduleId = nodeModules[nodeNum] ? nodeModules[nodeNum]->getId() : -1;

        // Add to dictionary
        moduleIDMap[nodeName] = moduleId;
        moduleGraphIDMap[nodeName] = nodeNum;

        // Write to file if loadFiles is false
        if (!loadFiles) {
            outFile << nodeName << " = ";
            if (moduleId != -1) {
                outFile << moduleId;
            } else {
                outFile << "NULL";
            }
            outFile << "\n";
        }
    }

    // Close file if loadFiles is false
    if (!loadFiles) {
        outFile.close();
        std::error_code renameError;
        fs::rename(temporaryPath, filePath, renameError);
        if (renameError) {
            fs::remove(temporaryPath);
            throw cRuntimeError("Could not atomically publish route ID map %s: %s",
                                filePath.c_str(), renameError.message().c_str());
        }
        EV << "Module ID mappings written to " << filePath << "\n";
    }

}

void LeoIpv4NetworkConfigurator::updateModuleIDMappingsClientServer()
{
    const int endpointStart = numOfSats + numOfGS;
    const int clientEnd = endpointStart + numOfClients;
    const int serverEnd = clientEnd + numOfClients;
    const int userTerminalEnd = serverEnd + numOfUserTerminals;

    for (int nodeNum = endpointStart; nodeNum < userTerminalEnd; ++nodeNum) {
        std::string nodeName;
        if (nodeNum < clientEnd)
            nodeName = "client[" + std::to_string(nodeNum - endpointStart) + "]";
        else if (nodeNum < serverEnd)
            nodeName = "server[" + std::to_string(nodeNum - clientEnd) + "]";
        else
            nodeName = "userTerminal[" + std::to_string(nodeNum - serverEnd) + "]";

        // Add to dictionary
        moduleGraphIDMap[nodeName] = nodeNum;
    }
}

void LeoIpv4NetworkConfigurator::verifyModuleIDMappingsFromFile(const std::string& filePath)
{
    if(!loadFiles){
       return;
    }

    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << " for reading.\n";
        return;
    }

    std::string line;
    bool allMatch = true;

    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::string nodeName;
        std::string equalsSign;
        std::string idStr;

        if (!(iss >> nodeName >> equalsSign >> idStr)) {
            std::cout << "Warning: Malformed line: " << line << "\n";
            continue;
        }

        // Remove potential trailing whitespace or newline characters
        nodeName.erase(nodeName.find_last_not_of(" \n\r\t") + 1);
        idStr.erase(idStr.find_last_not_of(" \n\r\t") + 1);

        int fileId = (idStr == "NULL") ? -1 : std::stoi(idStr);

        // Find in private map
        auto it = moduleIDMap.find(nodeName);
        if (it == moduleIDMap.end()) {
            std::cout << "Mismatch: " << nodeName << " not found in internal map.\n";
            allMatch = false;
        } else if (it->second != fileId) {
            std::cout << "Mismatch: " << nodeName << " = " << it->second << " (in map), "
               << fileId << " (in file)\n";
            allMatch = false;
        }
    }

    inFile.close();

    if (allMatch) {
        std::cout << "Module ID mappings in file match internal map.\n";
    }
}

int LeoIpv4NetworkConfigurator::getNextHopInterfaceID(cModule* sourceSatellite, cModule* nextHopSatellite)
{
    int interfaceID = -1;
    if(sourceSatellite == nextHopSatellite){
        return interfaceID;
    }

    IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(sourceSatellite->getSubmodule("interfaceTable"));
    IInterfaceTable* nextHopIft = dynamic_cast<IInterfaceTable*>(nextHopSatellite->getSubmodule("interfaceTable"));

    for (int i = 0; i < sourceIft->getNumInterfaces(); i++) {
        NetworkInterface *srcIE = sourceIft->getInterface(i);
        if (!(srcIE->isPointToPoint()))
            continue;
        cGate* srcGateMod = sourceSatellite->gate(srcIE->getNodeOutputGateId());
        for (int j = 0; j < nextHopIft->getNumInterfaces(); j++) {
            NetworkInterface *nextHopIE = nextHopIft->getInterface(j);
            if (!(nextHopIE->isPointToPoint()))
                continue;
            cGate* nextHopGateMod = nextHopSatellite->gate(nextHopIE->getNodeInputGateId());
            if(srcGateMod->getPathEndGate() == nextHopGateMod->getPathEndGate()){
                return srcIE->getInterfaceId();
            }
        }
    }
    return interfaceID;
}

void LeoIpv4NetworkConfigurator::fillNextHopInterfaceMap()
{
    for(int nodeNum = 0; nodeNum < nodeModules.size(); nodeNum++){
        cModule *mod = nodeModules[nodeNum];
        for(int nextHopNodeNum = 0; nextHopNodeNum < nodeModules.size(); nextHopNodeNum++){
            if(nodeNum != nextHopNodeNum){
                cModule *nextHopMod = nodeModules[nextHopNodeNum];
                IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(mod->getSubmodule("interfaceTable"));
                IInterfaceTable* nextHopIft = dynamic_cast<IInterfaceTable*>(nextHopMod->getSubmodule("interfaceTable"));

                for (int i = 0; i < sourceIft->getNumInterfaces(); i++) {
                    NetworkInterface *srcIE = sourceIft->getInterface(i);
                    if (!(srcIE->isPointToPoint())){
                        addIpAddressMap(srcIE->getIpv4Address().getInt(), mod->getFullName());
                        continue;
                    }
                    cGate* srcGateMod = mod->gate(srcIE->getNodeOutputGateId());
                    for (int j = 0; j < nextHopIft->getNumInterfaces(); j++) {
                        NetworkInterface *nextHopIE = nextHopIft->getInterface(j);
                        if (!(nextHopIE->isPointToPoint())){
                            continue;
                        }
                        cGate* nextHopGateMod = nextHopMod->gate(nextHopIE->getNodeInputGateId());
                        if(srcGateMod->getPathEndGate() == nextHopGateMod->getPathEndGate()){
                            addIpAddressMap(srcIE->getIpv4Address().getInt(), mod->getFullName());
                            if(nodeNum < numOfSats+numOfGS){
                                nextHopInterfaces.set(nodeNum, nextHopNodeNum, srcIE->getInterfaceId());
                            }
                        }
                    }
                }
            }
        }
    }
}

double LeoIpv4NetworkConfigurator::computeLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
        return computeWiredLinkWeight(link, metric, nullptr);
}

double LeoIpv4NetworkConfigurator::computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
    //std::cout << "\n Metric: " << metric << endl;
    Topology::Link *linkOut = static_cast<Topology::Link *>(static_cast<Topology::Link *>(link));
    if (!strcmp(metric, "hopCount"))
        return 1;
    else if (!strcmp(metric, "delay")) {
        cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLinkOutLocalGate()->findTransmissionChannel());
        if (transmissionChannel != nullptr){
            return transmissionChannel->getDelay().dbl();
        }
        else
            return minLinkWeight;
    }
    else if (!strcmp(metric, "dataRate")) {
        cChannel *transmissionChannel = linkOut->getLinkOutLocalGate()->findTransmissionChannel();
        if (transmissionChannel != nullptr) {
            double dataRate = transmissionChannel->getNominalDatarate();
            return dataRate != 0 ? 1 / dataRate : minLinkWeight;
        }
        else
            return minLinkWeight;
    }
    else if (!strcmp(metric, "errorRate")) {
        cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLinkOutLocalGate()->findTransmissionChannel());
        if (transmissionChannel != nullptr) {
            InterfaceInfo *sourceInterfaceInfo = link->sourceInterfaceInfo;
            double bitErrorRate = transmissionChannel->getBitErrorRate();
            double packetErrorRate = 1.0 - pow(1.0 - bitErrorRate, sourceInterfaceInfo->networkInterface->getMtu());
            return minLinkWeight - log(1 - packetErrorRate);
        }
        else
            return minLinkWeight;
    }
    else
        throw cRuntimeError("Unknown metric");
}

int LeoIpv4NetworkConfigurator::getModuleIdFromIpAddress(int address)
{
    auto it = routerIdMap.find(address);
    return it == routerIdMap.end() ? -1 : it->second;
}

void LeoIpv4NetworkConfigurator::addIpAddressMap(int address, std::string modName)
{
    auto it = moduleGraphIDMap.find(modName);
    if (it != moduleGraphIDMap.end())
        routerIdMap[address] = it->second;
}

void LeoIpv4NetworkConfigurator::eraseIpAddressMap(int address)
{
    routerIdMap.erase(address);
}

int LeoIpv4NetworkConfigurator::getNodeModuleGraphId(std::string nodeStr)
{
    auto it = moduleGraphIDMap.find(nodeStr);
    return it == moduleGraphIDMap.end() ? -1 : it->second;
}

void LeoIpv4NetworkConfigurator::addIpv4NextHop(cModule* mod, int destAddr, int nextHopId)
{
    LeoIpv4* ipv4Mod = dynamic_cast<LeoIpv4*>(mod->getModuleByPath(".ipv4.ip"));
    ipv4Mod->addKNextHop(1, destAddr, nextHopId);
    ipv4Mod->addNextHop(destAddr, nextHopId);
}

int LeoIpv4NetworkConfigurator::getGroundStationFromEndPoint(int endPointModID)
{
    auto it = endpointToNodeMap.find(endPointModID);
    if (it != endpointToNodeMap.end()) {
        return it->second;
    }
    return -1;
}

// 0 = sat, 1 = gs, 2 = endpoint, -1 = unknown
int LeoIpv4NetworkConfigurator::getNodeTypeCode(int modId)
{
    if (modId < 0) return -1;

    if (modId < numOfSats) //Satellite
        return 0;
    else if (modId < numOfSats + numOfGS) //Ground Station
        return 1;
    else if (modId < numOfSats + numOfGS + (numOfClients*2) + numOfUserTerminals) // Client, Server, or User Terminal
        return 2;
    else
        return -1;
}

void LeoIpv4NetworkConfigurator::setIpv4NodeIds()
{
    for (size_t id = 0; id < nodeModules.size(); id++) {
        cModule *mod = nodeModules[id];
        if (mod == nullptr)
            continue;

        LeoIpv4* ipv4Mod = ipv4Modules[id];
        if (ipv4Mod == nullptr) {
            cModule* ipModule = mod->getModuleByPath(".ipv4.ip");
            if (!ipModule)
                continue;
            ipv4Mod = dynamic_cast<LeoIpv4*>(ipModule);
            ipv4Modules[id] = ipv4Mod;
        }
        if (ipv4Mod != nullptr)
            ipv4Mod->setNodeId(id);
    }
}

void LeoIpv4NetworkConfigurator::setGroundStationsWithEndpoints()
{
    const size_t endpointStart = std::min(static_cast<size_t>(numOfSats + numOfGS),
                                          nodeModules.size());
    const auto previousEndpointToNodeMap = endpointToNodeMap;
    endpointToNodeMap.clear();
    endpointAttachmentInterfaceIds.clear();
    endpointUplinkInterfaceIds.clear();
    for (size_t modId = 0; modId < nodeModules.size(); modId++) {
        nodeNumEndpointsMap[modId] = 0;
    }

    for (size_t modId = endpointStart; modId < nodeModules.size(); modId++) {
        cModule *modulePtr = nodeModules[modId];
        if (modulePtr == nullptr)
            continue;

        for (int gateIndex = 0; gateIndex < modulePtr->gateSize("pppg$o"); gateIndex++) {
            cGate *sourceGate = modulePtr->gate("pppg$o", gateIndex);
            if (!sourceGate->isConnected())
                continue;

            IInterfaceTable *endpointIft = check_and_cast<IInterfaceTable *>(modulePtr->getSubmodule("interfaceTable"));
            NetworkInterface *endpointInterface = endpointIft->findInterfaceByNodeOutputGateId(sourceGate->getId());
            if (endpointInterface == nullptr)
                continue;

            cGate *endGate = sourceGate->getPathEndGate();
            if (endGate == nullptr)
                continue;

            cModule *attachedModule = nullptr;
            NetworkInterface *attachedInterface = nullptr;
            if (std::string(endGate->getBaseName()) == "pppg") {
                attachedModule = endGate->getOwnerModule();
                IInterfaceTable *attachedIft = check_and_cast<IInterfaceTable *>(attachedModule->getSubmodule("interfaceTable"));
                attachedInterface = attachedIft->findInterfaceByNodeInputGateId(endGate->getId());
            }
            else {
                cModule *pppModule = endGate->getOwnerModule();
                cModule *interfaceModule = pppModule != nullptr ? pppModule->getParentModule() : nullptr;
                attachedModule = interfaceModule != nullptr ? interfaceModule->getParentModule() : nullptr;
                attachedInterface = dynamic_cast<NetworkInterface *>(interfaceModule);
            }

            if (attachedModule == nullptr || attachedInterface == nullptr)
                continue;

            int attachedNodeId = getNodeModuleGraphId(attachedModule->getFullName());
            if (attachedNodeId < 0)
                continue;

            endpointToNodeMap[modId] = attachedNodeId;
            nodeNumEndpointsMap[attachedNodeId] = nodeNumEndpointsMap[attachedNodeId] + 1;
            endpointAttachmentInterfaceIds[modId] = attachedInterface->getInterfaceId();
            endpointUplinkInterfaceIds[modId] = endpointInterface->getInterfaceId();
            break;
        }
    }
    if (endpointToNodeMap != previousEndpointToNodeMap) {
        endpointAttachmentGeneration++;
        currentKPathGroups.clear();
    }
}

double LeoIpv4NetworkConfigurator::getEndpointAccessOneWayDelayMs(int nodeId) const
{
    if (nodeId < 0 || nodeId >= static_cast<int>(nodeModules.size()) ||
        nodeId < static_cast<int>(numOfSats + numOfGS))
        return 0;

    cModule *endpoint = nodeModules[nodeId];
    if (endpoint == nullptr)
        throw cRuntimeError("Endpoint node %d does not have a module", nodeId);

    for (int gateIndex = 0; gateIndex < endpoint->gateSize("pppg$o"); gateIndex++) {
        cGate *outputGate = endpoint->gate("pppg$o", gateIndex);
        if (!outputGate->isConnected())
            continue;
        cDatarateChannel *channel = dynamic_cast<cDatarateChannel *>(outputGate->findTransmissionChannel());
        if (channel != nullptr)
            return channel->getDelay().dbl() * 1000;
    }
    throw cRuntimeError("Endpoint node %d does not have an active access channel", nodeId);
}

std::vector<leoRouting::KPathEndpointState>
LeoIpv4NetworkConfigurator::getConfiguredKPathEndpointStates() const
{
    std::vector<int32_t> endpointIds;
    endpointIds.reserve(configuredKPathEndpointPairs.size() * 2);
    for (const auto& [source, destination] : configuredKPathEndpointPairs) {
        endpointIds.push_back(source);
        endpointIds.push_back(destination);
    }
    std::sort(endpointIds.begin(), endpointIds.end());
    endpointIds.erase(std::unique(endpointIds.begin(), endpointIds.end()), endpointIds.end());

    std::vector<leoRouting::KPathEndpointState> states;
    states.reserve(endpointIds.size());
    for (int32_t endpointId : endpointIds) {
        leoRouting::KPathEndpointState state;
        state.endpointNodeId = endpointId;
        auto attachment = endpointToNodeMap.find(endpointId);
        if (attachment != endpointToNodeMap.end()) {
            state.coreNodeId = attachment->second;
            state.accessOneWayDelayMs = getEndpointAccessOneWayDelayMs(endpointId);
        }
        states.push_back(state);
    }
    return states;
}

leoRouting::KShortestPathGroup LeoIpv4NetworkConfigurator::computeCanonicalKShortestPathGroup(
    int32_t sourceEndpoint, int32_t destinationEndpoint) const
{
    const auto canonicalPair = leoRouting::normalizeKPathEndpointPair(sourceEndpoint, destinationEndpoint);
    leoRouting::KShortestPathGroup group;
    group.sourceNodeId = canonicalPair.first;
    group.destinationNodeId = canonicalPair.second;
    group.requestedPathCount = numOfKPaths;
    group.maxRttSpreadMs = kPathMaxRttSpreadMs;
    group.edgeDisjoint = kPathsEdgeDisjoint;
    group.topologyGeneration = pathTopologyGeneration;
    group.endpointAttachmentGeneration = endpointAttachmentGeneration;

    auto sourceAttachment = endpointToNodeMap.find(group.sourceNodeId);
    auto destinationAttachment = endpointToNodeMap.find(group.destinationNodeId);
    if (sourceAttachment == endpointToNodeMap.end() || destinationAttachment == endpointToNodeMap.end()) {
        group.coreSourceNodeId = sourceAttachment == endpointToNodeMap.end() ? -1 : sourceAttachment->second;
        group.coreDestinationNodeId = destinationAttachment == endpointToNodeMap.end() ? -1 : destinationAttachment->second;
        return group;
    }

    group.coreSourceNodeId = sourceAttachment->second;
    group.coreDestinationNodeId = destinationAttachment->second;

    const double accessOneWayDelayMs = getEndpointAccessOneWayDelayMs(group.sourceNodeId) +
                                       getEndpointAccessOneWayDelayMs(group.destinationNodeId);
    const leoRouting::KShortestPathOptions options = {
        numOfKPaths,
        kPathMaxRttSpreadMs,
        kPathsEdgeDisjoint,
    };
    group.paths = currentPathTopology.findPaths(group.coreSourceNodeId, group.coreDestinationNodeId,
                                                options, accessOneWayDelayMs);
    for (leoRouting::KShortestPath& path : group.paths) {
        path.nodeIds.insert(path.nodeIds.begin(), group.sourceNodeId);
        path.nodeIds.push_back(group.destinationNodeId);
    }
    return group;
}

void LeoIpv4NetworkConfigurator::generateKPathSnapshot(
    simtime_t interval, const leoRouting::StableRouteState& routeState)
{
    if (!currentPathTopology.isInitialized())
        throw cRuntimeError("Cannot generate endpoint K paths before the weighted topology is initialized");
    const int32_t sequence = routeState.hasSequenceMetadata() ?
        routeState.sequence() : nextKPathSnapshotSequence;
    if (sequence != nextKPathSnapshotSequence)
        throw cRuntimeError("Endpoint K-path generation expected sequence %d but primary route state has %d",
                            nextKPathSnapshotSequence, sequence);

    std::vector<leoRouting::KShortestPathGroup> groups;
    groups.reserve(configuredKPathEndpointPairs.size());
    for (const auto& [source, destination] : configuredKPathEndpointPairs)
        groups.push_back(computeCanonicalKShortestPathGroup(source, destination));

    leoRouting::KPathSnapshotHeader header;
    header.sequence = sequence;
    header.timestampMicros = leoRouting::parseTimestampMicros(interval.str());
    header.routableNodeCount = routeState.sourceCount();
    header.totalNodeCount = static_cast<int32_t>(nodeModules.size());
    header.maxPathCount = numOfKPaths;
    header.algorithm = leoRouting::kPathAlgorithmForPolicy(kPathsEdgeDisjoint);
    header.maxRttSpreadMs = kPathMaxRttSpreadMs;
    header.edgeDisjoint = kPathsEdgeDisjoint;
    header.routeStateHash = routeState.stateHash();
    header.endpointStateHash = leoRouting::computeKPathEndpointStateHash(
        getConfiguredKPathEndpointStates());
    leoRouting::writeKPathSnapshotAtomic(getKPathSnapshotPath(interval), header, groups,
                                         allowRouteSnapshotOverwrite);

    std::unordered_map<uint64_t, leoRouting::KShortestPathGroup> candidateCatalog;
    candidateCatalog.reserve(groups.size());
    for (auto& group : groups)
        candidateCatalog.emplace(leoRouting::kPathEndpointPairKey(group.sourceNodeId, group.destinationNodeId),
                                 std::move(group));
    currentKPathGroups.swap(candidateCatalog);
    nextKPathSnapshotSequence++;
}

void LeoIpv4NetworkConfigurator::loadKPathSnapshot(
    simtime_t interval, const leoRouting::StableRouteState& routeState)
{
    const std::filesystem::path path = getKPathSnapshotPath(interval);
    leoRouting::KPathSnapshot snapshot;
    try {
        snapshot = leoRouting::readKPathSnapshot(path);
    }
    catch (const std::exception& error) {
        throw cRuntimeError("Failed to load endpoint K-path snapshot %s: %s",
                            path.string().c_str(), error.what());
    }

    const int32_t expectedSequence = routeState.hasSequenceMetadata() ?
        routeState.sequence() : nextKPathSnapshotSequence;
    const int32_t routableNodeCount = std::min(static_cast<int32_t>(numOfSats + numOfGS),
                                               static_cast<int32_t>(nodeModules.size()));
    if (snapshot.header.sequence != expectedSequence)
        throw cRuntimeError("Endpoint K-path snapshot %s has sequence %d; expected %d",
                            path.string().c_str(), snapshot.header.sequence, expectedSequence);
    if (snapshot.header.routableNodeCount != routableNodeCount ||
        snapshot.header.totalNodeCount != static_cast<int32_t>(nodeModules.size()))
        throw cRuntimeError("Endpoint K-path snapshot %s does not match the current node dimensions",
                            path.string().c_str());
    if (snapshot.header.maxPathCount != numOfKPaths ||
        snapshot.header.algorithm != leoRouting::kPathAlgorithmForPolicy(kPathsEdgeDisjoint) ||
        snapshot.header.maxRttSpreadMs != kPathMaxRttSpreadMs ||
        snapshot.header.edgeDisjoint != kPathsEdgeDisjoint)
        throw cRuntimeError("Endpoint K-path snapshot %s does not match the configured K-path policy",
                            path.string().c_str());
    if (snapshot.header.routeStateHash != routeState.stateHash())
        throw cRuntimeError("Endpoint K-path snapshot %s was generated for a different primary route state",
                            path.string().c_str());
    const uint64_t endpointStateHash = leoRouting::computeKPathEndpointStateHash(
        getConfiguredKPathEndpointStates());
    if (snapshot.header.endpointStateHash != endpointStateHash)
        throw cRuntimeError("Endpoint K-path snapshot %s does not match the current endpoint attachments or access delays",
                            path.string().c_str());
    if (snapshot.groups.size() != configuredKPathEndpointPairs.size())
        throw cRuntimeError("Endpoint K-path snapshot %s contains %zu pairs; expected %zu",
                            path.string().c_str(), snapshot.groups.size(),
                            configuredKPathEndpointPairs.size());

    std::unordered_map<uint64_t, leoRouting::KShortestPathGroup> candidateCatalog;
    candidateCatalog.reserve(snapshot.groups.size());
    for (auto& group : snapshot.groups) {
        const uint64_t pairKey = leoRouting::kPathEndpointPairKey(group.sourceNodeId,
                                                                  group.destinationNodeId);
        candidateCatalog.emplace(pairKey, std::move(group));
    }
    for (const auto& [source, destination] : configuredKPathEndpointPairs) {
        const uint64_t pairKey = leoRouting::kPathEndpointPairKey(source, destination);
        auto group = candidateCatalog.find(pairKey);
        if (group == candidateCatalog.end())
            throw cRuntimeError("Endpoint K-path snapshot %s is missing configured pair (%d,%d)",
                                path.string().c_str(), source, destination);
        const int32_t expectedSourceCore = endpointToNodeMap.count(source) ? endpointToNodeMap.at(source) : -1;
        const int32_t expectedDestinationCore = endpointToNodeMap.count(destination) ? endpointToNodeMap.at(destination) : -1;
        if (group->second.coreSourceNodeId != expectedSourceCore ||
            group->second.coreDestinationNodeId != expectedDestinationCore) {
            throw cRuntimeError("Endpoint K-path snapshot %s has stale attachments for pair (%d,%d)",
                                path.string().c_str(), source, destination);
        }
    }

    pathTopologyGeneration++;
    for (auto& [pairKey, group] : candidateCatalog) {
        group.topologyGeneration = pathTopologyGeneration;
        group.endpointAttachmentGeneration = endpointAttachmentGeneration;
        kPathGroupsLoaded++;
        kPathsLoaded += group.paths.size();
        for (const leoRouting::KShortestPath& savedPath : group.paths)
            kPathNodeIdsLoaded += savedPath.nodeIds.size();
    }
    kPathSnapshotFilesRead++;
    kPathSnapshotBytesRead += snapshot.bytesRead;
    currentKPathGroups.swap(candidateCatalog);
    nextKPathSnapshotSequence++;
}

leoRouting::KShortestPathGroup LeoIpv4NetworkConfigurator::getKShortestPathGroup(
    int sourceNodeId, int destinationNodeId, int requestedPathCount) const
{
    if (kPathSnapshotMode == KPathSnapshotMode::Disabled)
        throw cRuntimeError("Endpoint K-path snapshots are disabled");
    const int pathCount = requestedPathCount < 0 ? numOfKPaths : requestedPathCount;
    if (pathCount < 1 || pathCount > numOfKPaths)
        throw cRuntimeError("Requested K-shortest path count %d is outside the configured range 1..%d",
                            pathCount, numOfKPaths);
    uint64_t pairKey;
    try {
        pairKey = leoRouting::kPathEndpointPairKey(sourceNodeId, destinationNodeId);
    }
    catch (const std::exception& error) {
        throw cRuntimeError("Invalid endpoint K-path query: %s", error.what());
    }
    auto savedGroup = currentKPathGroups.find(pairKey);
    if (savedGroup == currentKPathGroups.end())
        throw cRuntimeError("No saved endpoint K-path group is configured for nodes %d and %d",
                            sourceNodeId, destinationNodeId);
    try {
        return leoRouting::orientKShortestPathGroup(savedGroup->second, sourceNodeId,
                                                    destinationNodeId, pathCount);
    }
    catch (const std::exception& error) {
        throw cRuntimeError("Failed to read saved endpoint K-path group: %s", error.what());
    }
}

leoRouting::KShortestPathGroup LeoIpv4NetworkConfigurator::getKShortestPathGroupForAddresses(
    int sourceAddress, int destinationAddress, int requestedPathCount) const
{
    auto source = routerIdMap.find(sourceAddress);
    if (source == routerIdMap.end())
        throw cRuntimeError("No LEO node mapping exists for source IPv4 address %d", sourceAddress);
    auto destination = routerIdMap.find(destinationAddress);
    if (destination == routerIdMap.end())
        throw cRuntimeError("No LEO node mapping exists for destination IPv4 address %d", destinationAddress);
    return getKShortestPathGroup(source->second, destination->second, requestedPathCount);
}

int LeoIpv4NetworkConfigurator::getEndpointAttachmentInterfaceId(int nodeId) {
    auto endPointInterface = endpointAttachmentInterfaceIds.find(nodeId);
    if(endPointInterface != endpointAttachmentInterfaceIds.end()){
        return endPointInterface->second;
    }
    return -1;
}

int LeoIpv4NetworkConfigurator::getEndpointUplinkInterfaceId(int nodeId) {
    auto endPointInterface = endpointUplinkInterfaceIds.find(nodeId);
    if(endPointInterface != endpointUplinkInterfaceIds.end()){
        return endPointInterface->second;
    }
    return -1;
}

int LeoIpv4NetworkConfigurator::getTotalEndpoints(int nodeId) {
    auto numEndpoints = nodeNumEndpointsMap.find(nodeId);
    if(numEndpoints != nodeNumEndpointsMap.end()){
        return numEndpoints->second;
    }
    return 0;
}
// TODO If we set IPAddress in LeoChannelConstructor, do we need this? doubt it
//void LeoIpv4NetworkConfigurator::configureInterface(InterfaceInfo *interfaceInfo)
//{
//    EV_DETAIL << "Configuring network interface " << interfaceInfo->getFullPath() << ".\n";
//    InterfaceEntry *interfaceEntry = interfaceInfo->interfaceEntry;
//    Ipv4InterfaceData *interfaceData = interfaceEntry->getProtocolData<Ipv4InterfaceData>();
//    if (interfaceInfo->mtu != -1)
//        interfaceEntry->setMtu(interfaceInfo->mtu);
//    if (interfaceInfo->metric != -1)
//        interfaceData->setMetric(interfaceInfo->metric);
//    if (assignAddressesParameter) {
//        interfaceData->setIPAddress(Ipv4Address(interfaceInfo->address));
//        interfaceData->setNetmask(Ipv4Address(interfaceInfo->netmask));
//    }
//    // TODO: should we leave joined multicast groups first?
//    for (auto & multicastGroup : interfaceInfo->multicastGroups)
//        interfaceData->joinMulticastGroup(multicastGroup);
//}
}
