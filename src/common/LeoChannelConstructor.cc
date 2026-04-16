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

#include <fstream> //check if needed
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <inet/common/INETUtils.h>
#include <inet/common/XMLUtils.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/lifecycle/LifecycleOperation.h>
#include <inet/common/lifecycle/ModuleOperations.h>

#include "LeoChannelConstructor.h"

using namespace std;
namespace inet {
Define_Module(LeoChannelConstructor);

static bool isEndUserTypeName(const std::string& typeName)
{
    return typeName == "leosatellites.base.EndUser";
}

static bool isUserTerminalTypeName(const std::string& typeName)
{
    return typeName == "leosatellites.base.UserTerminal";
}

LeoChannelConstructor::~LeoChannelConstructor() {
    cancelAndDelete(startManagerNode);
    cancelAndDelete(updateTimer);
    cancelAndDelete(userTerminalHandoverTimer);
}

LeoChannelConstructor::LeoChannelConstructor(){
    numOfSats = 0;
    numOfGS = 0;
    numOfPlanes = 0;
    numOfClients = 0;
    numOfUserTerminals = 0;
    satPerPlane = 0;
    startManagerNode = nullptr;
    updateTimer = nullptr;
    userTerminalHandoverTimer = nullptr;
    updateInterval = 0;
    userTerminalUpdateInterval = 0;
    userTerminalSampleInterval = 0;
    userTerminalHandoverDowntime = 0;
    nextUserTerminalUpdate = 0;
    linkDataRate = 0;
}

void LeoChannelConstructor::initialize(int stage)
{
    EV_TRACE << "initializing LeoChannelConstructor stage " << stage << endl;

    if (stage == INITSTAGE_LOCAL) {
        // Read base IP address and subnet mask from parameters
        addressBase = Ipv4Address(par("addressBase").stringValue());
        netmask = Ipv4Address(par("netmask").stringValue());

        // Create timer messages
        startManagerNode = new cMessage("startManagerNode");
        updateTimer = new cMessage("update");
        userTerminalHandoverTimer = new cMessage("userTerminalHandover");

        // Get reference to the parent module
        cModule *parent = getParentModule();

        // Retrieve satellite network parameters from parent module
        int planes = parent->par("numOfPlanes");
        numOfSats = parent->par("numOfSats");
        numOfGS = parent->par("numOfGS");
        numOfClients = parent->par("numOfClients");
        numOfUserTerminals = parent->par("numOfUserTerminals");
        satPerPlane = parent->par("satsPerPlane");
        enableInterSatelliteLinks = parent->par("enableInterSatelliteLinks").boolValue();

        // Calculate total number of planes, accounting for unfilled ones
        numOfPlanes = (int)std::ceil(((double)numOfSats / ((double)planes * (double)satPerPlane)) * (double)planes);

        // Get reference to the configurator submodule
        configurator = dynamic_cast<LeoIpv4NetworkConfigurator*>(parent->getSubmodule("configurator"));

        // Initialize update and interval counters
        updateInterval = 0;
        currentInterval = 0;
        userTerminalUpdateInterval = par("userTerminalUpdateInterval");
        userTerminalSampleInterval = par("userTerminalSampleInterval");
        userTerminalHandoverDowntime = par("userTerminalHandoverDowntime");
        nextUserTerminalUpdate = 0;

        // Get general network configuration
        networkName = parent->getName();
        linkDataRate = par("dataRate").doubleValue();
        queueSize = par("queueSize");
        interfaceType = par("interfaceType").stringValue();
        cacheNetworkModules();

        // Start manager node at simulation time 0
        scheduleAt(0, startManagerNode);
    }
}

//TODO move the message handling to configurator?
void LeoChannelConstructor::handleMessage(cMessage *msg)
{
    if(msg == updateTimer){
        setUpGSLinks();
        const bool userTerminalLinksChanged = refreshUserTerminalLinks(false);
        addPPPInterfaces();
        if (userTerminalLinksChanged)
            finalizeUserTerminalLinks();
        //setUpInterfaces();
        updateChannels();
        configurator->updateForwardingStates(simTime());

        //update all routing tables
        //perform global arp

        //configurator->configure();
        scheduleUpdate(false);
    }
    else if (msg == userTerminalHandoverTimer) {
        const bool userTerminalLinksChanged = refreshUserTerminalLinks(false);
        addPPPInterfaces();
        if (userTerminalLinksChanged)
            finalizeUserTerminalLinks();
        updateChannels();
    }
    else if(msg == startManagerNode){
        setUpSimulation();
        setUpGSLinks();
        refreshUserTerminalLinks(true);

        setUpInterfaces();
        updateChannels();

        if(enableInterSatelliteLinks){
            configurator->establishInitialISLs();
        }

        configurator->fillNextHopInterfaceMap();

        configurator->updateForwardingStates(simTime());

        configurator->setIpv4NodeIds();

        configurator->setGroundStationsWithEndpoints();

        scheduleUpdate(true);
    }
    else{
        throw cRuntimeError("Unknown message arrived at channel constructor.");
    }
}

void LeoChannelConstructor::cacheNetworkModules()
{
    satelliteModules.resize(numOfSats);
    satelliteMobilities.resize(numOfSats);
    satelliteNoradModules.resize(numOfSats);
    for (int satNum = 0; satNum < numOfSats; satNum++) {
        std::string satName = networkName + ".satellite[" + std::to_string(satNum) + "]";
        cModule *satModule = getModuleByPath(satName.c_str());
        satelliteModules[satNum] = satModule;
        satelliteMobilities[satNum] = satModule ? check_and_cast<SatelliteMobility *>(satModule->getSubmodule("mobility")) : nullptr;
        satelliteNoradModules[satNum] = satModule ? check_and_cast<INorad *>(satModule->getSubmodule("NoradModule")) : nullptr;
    }

    groundStationModules.resize(numOfGS);
    groundStationMobilities.resize(numOfGS);
    for (int gsNum = 0; gsNum < numOfGS; gsNum++) {
        std::string gsName = networkName + ".groundStation[" + std::to_string(gsNum) + "]";
        cModule *gsModule = getModuleByPath(gsName.c_str());
        groundStationModules[gsNum] = gsModule;
        groundStationMobilities[gsNum] = gsModule ? check_and_cast<GroundStationMobility *>(gsModule->getSubmodule("mobility")) : nullptr;
    }

    clientModules.resize(numOfClients);
    serverModules.resize(numOfClients);
    for (int clientNum = 0; clientNum < numOfClients; clientNum++) {
        std::string clientName = networkName + ".client[" + std::to_string(clientNum) + "]";
        std::string serverName = networkName + ".server[" + std::to_string(clientNum) + "]";
        clientModules[clientNum] = getModuleByPath(clientName.c_str());
        serverModules[clientNum] = getModuleByPath(serverName.c_str());
    }

    userTerminalModules.resize(numOfUserTerminals);
    userTerminalMobilities.resize(numOfUserTerminals);
    for (int userTerminalNum = 0; userTerminalNum < numOfUserTerminals; userTerminalNum++) {
        std::string userTerminalName = networkName + ".userTerminal[" + std::to_string(userTerminalNum) + "]";
        cModule *userTerminalModule = getModuleByPath(userTerminalName.c_str());
        userTerminalModules[userTerminalNum] = userTerminalModule;
        userTerminalMobilities[userTerminalNum] = userTerminalModule ? check_and_cast<GroundStationMobility *>(userTerminalModule->getSubmodule("mobility")) : nullptr;
    }
}

void LeoChannelConstructor::scheduleUpdate(bool simStart)
{
    cancelEvent(updateTimer);
    simtime_t nextUpdate;
    if(!simStart){
        nextUpdate = simTime() + updateInterval;
    }
    else{
        nextUpdate = simTime() + updateInterval + SimTime(1, SIMTIME_US);
    }
    scheduleAt(nextUpdate, updateTimer);
}

void LeoChannelConstructor::setUpSimulation()
{
    const std::vector<cModule *> endUsers = [&]() {
        std::vector<cModule *> modules;
        modules.reserve(clientModules.size() + serverModules.size());
        modules.insert(modules.end(), clientModules.begin(), clientModules.end());
        modules.insert(modules.end(), serverModules.begin(), serverModules.end());
        return modules;
    }();

    for (cModule *module : endUsers) {
        if (!module)
            continue;
        std::string connectName = module->par("connectModule");
        std::string destFullName = networkName + "." + connectName;
        cModule *destMod = getModuleByPath(destFullName.c_str());

        std::pair<cGate*, cGate*> gatePair1 = getNextFreeGate(module);
        std::pair<cGate*, cGate*> gatePair2 = getNextFreeGate(destMod);

        cGate* inGate = gatePair1.first;
        cGate* outGate = gatePair1.second;
        cGate* inGateDest = gatePair2.first;
        cGate* outGateDest = gatePair2.second;

        createChannel(0, outGate, inGateDest, true);
        createChannel(0, outGateDest, inGate, true);
    }

    if (!satelliteMobilities.empty() && satelliteMobilities.front() != nullptr) {
        updateInterval = satelliteMobilities.front()->par("updateInterval").doubleValue();
    }

    if(!enableInterSatelliteLinks){
        return;
    }

    for(int planeNum = 0; planeNum < numOfPlanes; planeNum++){
        unsigned int numOfSatsInPlane =  planeNum*satPerPlane+satPerPlane;
        if(numOfSats < numOfSatsInPlane){
            numOfSatsInPlane = numOfSats;
        }
        for(unsigned int satNum = planeNum*satPerPlane; satNum < numOfSatsInPlane; satNum++){
            cModule *satMod = satelliteModules[satNum];
            SatelliteMobility *sourceSatMobility = satelliteMobilities[satNum];

            cGate *inGateSat1;
            cGate *outGateSat1;
            cGate *inGateSat2;
            cGate *outGateSat2;
            int destSatNumA = (satNum+1)%(satPerPlane*(planeNum+1));
            if(destSatNumA == 0){
                destSatNumA = planeNum*satPerPlane; //If number is zero, must be start of orbital plane
            }

            if(destSatNumA < numOfSats){
                cModule *destModA = satelliteModules[destSatNumA];
                std::pair <cGate*, cGate*> gatePair1 = getNextFreeGate(satMod);
                std::pair <cGate*, cGate*> gatePair2 = getNextFreeGate(destModA);

                inGateSat1 = gatePair1.first;
                outGateSat1 = gatePair1.second;
                inGateSat2 = gatePair2.first;
                outGateSat2 = gatePair2.second;

                SatelliteMobility* destSatMobility = satelliteMobilities[destSatNumA];
                double delaySeconds = (sourceSatMobility->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude()) * 1000.0) / 299792458.0;
                createChannel(delaySeconds, outGateSat1, inGateSat2, false);
                createChannel(delaySeconds, outGateSat2, inGateSat1, false);
                fixedTimedLinks.push_back({sourceSatMobility, destSatMobility, nullptr,
                                           check_and_cast<cDatarateChannel *>(outGateSat1->getChannel()),
                                           check_and_cast<cDatarateChannel *>(outGateSat2->getChannel()),
                                           static_cast<int>(satNum), -1, false});
            }

            int destSatNumB = (satNum + satPerPlane);// % totalSats;
            if(destSatNumB < numOfSats){
                cModule *destModB = satelliteModules[destSatNumB];
                std::pair <cGate*, cGate*> gatePair1 = getNextFreeGate(satMod);
                std::pair <cGate*, cGate*> gatePair2 = getNextFreeGate(destModB);

                inGateSat1 = gatePair1.first;
                outGateSat1 = gatePair1.second;
                inGateSat2 = gatePair2.first;
                outGateSat2 = gatePair2.second;

                SatelliteMobility* destSatMobility = satelliteMobilities[destSatNumB];
                double delaySeconds = (sourceSatMobility->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude()) * 1000.0) / 299792458.0;
                createChannel(delaySeconds, outGateSat1, inGateSat2, false);
                createChannel(delaySeconds, outGateSat2, inGateSat1, false);
                fixedTimedLinks.push_back({sourceSatMobility, destSatMobility, nullptr,
                                           check_and_cast<cDatarateChannel *>(outGateSat1->getChannel()),
                                           check_and_cast<cDatarateChannel *>(outGateSat2->getChannel()),
                                           static_cast<int>(satNum), -1, false});
            }
        }
    }
}

void LeoChannelConstructor::setUpClientServerInterfaces()
{
    for(int clientNum = 0; clientNum < numOfClients; clientNum++){
        cModule *clientMod = clientModules[clientNum];
        NetworkInterface* clientInterface = dynamic_cast<NetworkInterface*>(clientMod->getSubmodule("ppp", 0));
        setUpIpLayer(clientMod, clientInterface);
        dynamic_cast<LeoIpv4RoutingTable*>(clientMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();

        cModule *serverMod = serverModules[clientNum];
        NetworkInterface* serverInterface = dynamic_cast<NetworkInterface*>(serverMod->getSubmodule("ppp", 0));
        setUpIpLayer(serverMod, serverInterface);
        dynamic_cast<LeoIpv4RoutingTable*>(serverMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }
}

void LeoChannelConstructor::setUpInterfaces()
{
    for (cModule *mod : clientModules) {
        updatePPPModules(mod, false);
        auto* routingTable = dynamic_cast<LeoIpv4RoutingTable*>(mod->getModuleByPath(".ipv4.routingTable"));
        if (routingTable)
            routingTable->configureRouterId();
    }

    for (cModule *mod : serverModules) {
        updatePPPModules(mod, false);
        auto* routingTable = dynamic_cast<LeoIpv4RoutingTable*>(mod->getModuleByPath(".ipv4.routingTable"));
        if (routingTable)
            routingTable->configureRouterId();
    }

    for (cModule *mod : userTerminalModules) {
        updatePPPModules(mod, false);
        auto *routingTable = dynamic_cast<LeoIpv4RoutingTable*>(mod->getModuleByPath(".ipv4.routingTable"));
        if (routingTable)
            routingTable->configureRouterId();
    }

    for(int satNum = 0; satNum < numOfSats; satNum++){
        cModule *satMod = satelliteModules[satNum];
        updatePPPModules(satMod, true);
        dynamic_cast<LeoIpv4RoutingTable*>(satMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }

    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        cModule *gsMod = groundStationModules[gsNum];
        updatePPPModules(gsMod, true);
        dynamic_cast<LeoIpv4RoutingTable*>(gsMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }
}

void LeoChannelConstructor::addPPPInterfaces(){
    if (dirtyPppModules.empty())
        return;

    for (cModule *mod : dirtyPppModules)
        updatePPPModules(mod, true);
    dirtyPppModules.clear();
}

std::pair<cGate*,cGate*> LeoChannelConstructor::getNextFreeGate(cModule *mod)
{
    cGate *inGate;
    cGate *outGate;
    bool foundFreeGate = false;
    while(!foundFreeGate){
        for(int i = 0; i < mod->gateSize("pppg"); i++){
                inGate = mod->gateHalf("pppg", cGate::INPUT, i);
                outGate = mod->gateHalf("pppg", cGate::OUTPUT, i);
                if(!inGate->isConnectedOutside() && !outGate->isConnectedOutside()){
                    foundFreeGate = true;
                    break;
                }
        }
        if(!foundFreeGate){
            mod->setGateSize("pppg", mod->gateSize("pppg")+1);
        }
    }
    return std::pair<cGate*, cGate*>(inGate, outGate);
}

void LeoChannelConstructor::updateChannels()
{
    configurator->clearGroundStationLinks();
    for (const auto& linkRecord : fixedTimedLinks)
        updateTimedLink(linkRecord);
    for (const auto& [key, activeLink] : activeGroundStationLinks)
        updateTimedLink(activeLink.linkRecord);
    for (const auto& [userTerminalIndex, activeLink] : activeUserTerminalLinks) {
        double delaySeconds = (activeLink.satelliteMobility->getDistance(activeLink.userTerminalMobility->getLUTPositionY(),
                                                                        activeLink.userTerminalMobility->getLUTPositionX(),
                                                                        0) * 1000.0) / 299792458.0;
        activeLink.forwardChannel->setDelay(delaySeconds);
        activeLink.forwardChannel->setDatarate(linkDataRate);
        activeLink.reverseChannel->setDelay(delaySeconds);
        activeLink.reverseChannel->setDatarate(linkDataRate);
    }
}

void LeoChannelConstructor::setUpGSLinks()
{
    //Sets up new GS links if new satellite is reachable.
    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        for(int satNum = 0; satNum < numOfSats; satNum++){
            const uint64_t linkKey = getGroundStationLinkKey(gsNum, satNum);
            const bool reachable = isGroundStationSatelliteReachableNow(gsNum, satNum);
            auto activeIt = activeGroundStationLinks.find(linkKey);

            if (reachable) {
                if (activeIt == activeGroundStationLinks.end())
                    createGroundStationLink(gsNum, satNum);
            }
            else if (activeIt != activeGroundStationLinks.end()) {
                removeGroundStationLink(gsNum, satNum, activeIt->second);
                activeGroundStationLinks.erase(activeIt);
            }
        }
    }
}

bool LeoChannelConstructor::refreshUserTerminalLinks(bool forceUpdate)
{
    if (userTerminalModules.empty()) {
        cancelEvent(userTerminalHandoverTimer);
        pendingUserTerminalHandovers.clear();
        return false;
    }

    bool linksReadyToFinalize = false;
    bool endpointTopologyChanged = false;

    auto removeActiveUserTerminalLink = [&](int userTerminalIndex, std::unordered_map<int, ActiveUserTerminalLink>::iterator activeLinkIt) {
        removeUserTerminalLink(userTerminalIndex, activeLinkIt->second);
        activeUserTerminalLinks.erase(activeLinkIt);
        pendingUserTerminalHandovers.erase(userTerminalIndex);
        endpointTopologyChanged = true;
    };

    auto schedulePendingHandover = [&](int userTerminalIndex, int targetSatelliteIndex) {
        PendingUserTerminalHandover pendingHandover;
        pendingHandover.userTerminalIndex = userTerminalIndex;
        pendingHandover.targetSatelliteIndex = targetSatelliteIndex;
        pendingHandover.reconnectTime = simTime() + userTerminalHandoverDowntime;
        pendingUserTerminalHandovers[userTerminalIndex] = pendingHandover;
    };

    if (forceUpdate) {
        pendingUserTerminalHandovers.clear();
        nextUserTerminalUpdate = userTerminalUpdateInterval > SIMTIME_ZERO ? simTime() + userTerminalUpdateInterval : SIMTIME_ZERO;
        for (int userTerminalIndex = 0; userTerminalIndex < numOfUserTerminals; userTerminalIndex++) {
            const int selectedSatellite = selectServingSatelliteForUserTerminal(userTerminalIndex);
            auto activeLinkIt = activeUserTerminalLinks.find(userTerminalIndex);
            if (activeLinkIt != activeUserTerminalLinks.end() && activeLinkIt->second.satelliteIndex == selectedSatellite)
                continue;
            if (activeLinkIt != activeUserTerminalLinks.end())
                removeActiveUserTerminalLink(userTerminalIndex, activeLinkIt);

            if (selectedSatellite >= 0 && isUserTerminalSatelliteReachableNow(userTerminalIndex, selectedSatellite)) {
                createUserTerminalLink(userTerminalIndex, selectedSatellite);
                linksReadyToFinalize = true;
            }
        }
        scheduleUserTerminalHandoverTimer();
        return linksReadyToFinalize;
    }

    // Drop links that have actually gone out of view, then keep probing for a
    // replacement so detached terminals don't wait until the next polling slot.
    for (int userTerminalIndex = 0; userTerminalIndex < numOfUserTerminals; userTerminalIndex++) {
        auto activeLinkIt = activeUserTerminalLinks.find(userTerminalIndex);
        if (activeLinkIt != activeUserTerminalLinks.end() &&
            !isUserTerminalSatelliteReachableNow(userTerminalIndex, activeLinkIt->second.satelliteIndex)) {
            removeActiveUserTerminalLink(userTerminalIndex, activeLinkIt);
        }

        if (activeUserTerminalLinks.find(userTerminalIndex) == activeUserTerminalLinks.end() &&
            pendingUserTerminalHandovers.find(userTerminalIndex) == pendingUserTerminalHandovers.end()) {
            const int selectedSatellite = selectServingSatelliteForUserTerminal(userTerminalIndex);
            if (selectedSatellite >= 0 && isUserTerminalSatelliteReachableNow(userTerminalIndex, selectedSatellite))
                schedulePendingHandover(userTerminalIndex, selectedSatellite);
        }
    }

    if (userTerminalUpdateInterval > SIMTIME_ZERO && simTime() >= nextUserTerminalUpdate) {
        for (int userTerminalIndex = 0; userTerminalIndex < numOfUserTerminals; userTerminalIndex++) {
            const int selectedSatellite = selectServingSatelliteForUserTerminal(userTerminalIndex);
            auto activeLinkIt = activeUserTerminalLinks.find(userTerminalIndex);
            const int currentSatellite = activeLinkIt != activeUserTerminalLinks.end() ? activeLinkIt->second.satelliteIndex : -1;
            const bool currentReachableNow = isUserTerminalSatelliteReachableNow(userTerminalIndex, currentSatellite);
            const bool selectedReachableNow = isUserTerminalSatelliteReachableNow(userTerminalIndex, selectedSatellite);

            if (activeLinkIt != activeUserTerminalLinks.end()) {
                if (selectedSatellite == currentSatellite) {
                    pendingUserTerminalHandovers.erase(userTerminalIndex);
                    continue;
                }

                // Keep using a healthy serving satellite until an immediate
                // replacement exists. Otherwise the polling interval can create
                // multi-second blackouts on its own.
                if ((selectedSatellite < 0 || !selectedReachableNow) && currentReachableNow) {
                    pendingUserTerminalHandovers.erase(userTerminalIndex);
                    continue;
                }

                removeActiveUserTerminalLink(userTerminalIndex, activeLinkIt);
            }

            if (selectedSatellite >= 0 && selectedReachableNow) {
                schedulePendingHandover(userTerminalIndex, selectedSatellite);
            }
            else {
                pendingUserTerminalHandovers.erase(userTerminalIndex);
            }
        }

        while (nextUserTerminalUpdate <= simTime())
            nextUserTerminalUpdate += userTerminalUpdateInterval;
    }

    linksReadyToFinalize = completePendingUserTerminalHandovers() || linksReadyToFinalize;
    scheduleUserTerminalHandoverTimer();

    if (endpointTopologyChanged && !linksReadyToFinalize)
        configurator->setGroundStationsWithEndpoints();

    return linksReadyToFinalize;
}

bool LeoChannelConstructor::completePendingUserTerminalHandovers()
{
    bool linksChanged = false;
    std::vector<int> completedUserTerminals;

    for (const auto& [userTerminalIndex, pendingHandover] : pendingUserTerminalHandovers) {
        if (pendingHandover.reconnectTime > simTime())
            continue;

        if (pendingHandover.targetSatelliteIndex >= 0 &&
            isUserTerminalSatelliteReachableNow(userTerminalIndex, pendingHandover.targetSatelliteIndex)) {
            createUserTerminalLink(userTerminalIndex, pendingHandover.targetSatelliteIndex);
            linksChanged = true;
        }
        completedUserTerminals.push_back(userTerminalIndex);
    }

    for (int userTerminalIndex : completedUserTerminals)
        pendingUserTerminalHandovers.erase(userTerminalIndex);

    return linksChanged;
}

bool LeoChannelConstructor::isGroundStationSatelliteReachableNow(int groundStationIndex, int satelliteIndex) const
{
    if (groundStationIndex < 0 || groundStationIndex >= numOfGS || satelliteIndex < 0 || satelliteIndex >= numOfSats)
        return false;

    GroundStationMobility *groundStationMobility = groundStationMobilities[groundStationIndex];
    INorad *noradModule = satelliteNoradModules[satelliteIndex];
    if (groundStationMobility == nullptr || noradModule == nullptr)
        return false;

    return noradModule->isReachableAtTime(simTime(),
                                          groundStationMobility->getLUTPositionY(),
                                          groundStationMobility->getLUTPositionX(),
                                          0);
}

bool LeoChannelConstructor::isUserTerminalSatelliteReachableNow(int userTerminalIndex, int satelliteIndex) const
{
    if (userTerminalIndex < 0 || userTerminalIndex >= numOfUserTerminals || satelliteIndex < 0 || satelliteIndex >= numOfSats)
        return false;

    GroundStationMobility *userTerminalMobility = userTerminalMobilities[userTerminalIndex];
    INorad *noradModule = satelliteNoradModules[satelliteIndex];
    if (userTerminalMobility == nullptr || noradModule == nullptr)
        return false;

    return noradModule->isReachableAtTime(simTime(),
                                          userTerminalMobility->getLUTPositionY(),
                                          userTerminalMobility->getLUTPositionX(),
                                          0);
}

void LeoChannelConstructor::scheduleUserTerminalHandoverTimer()
{
    cancelEvent(userTerminalHandoverTimer);

    simtime_t nextReconnectTime = SIMTIME_ZERO;
    bool foundPendingReconnect = false;
    for (const auto& [userTerminalIndex, pendingHandover] : pendingUserTerminalHandovers) {
        if (!foundPendingReconnect || pendingHandover.reconnectTime < nextReconnectTime) {
            nextReconnectTime = pendingHandover.reconnectTime;
            foundPendingReconnect = true;
        }
    }

    if (foundPendingReconnect)
        scheduleAt(nextReconnectTime, userTerminalHandoverTimer);
}

int LeoChannelConstructor::selectServingSatelliteForUserTerminal(int userTerminalIndex)
{
    GroundStationMobility *userTerminalMobility = userTerminalMobilities[userTerminalIndex];
    if (userTerminalMobility == nullptr)
        return -1;

    const simtime_t slotStart = simTime();
    const simtime_t slotEnd = slotStart + userTerminalUpdateInterval;
    const auto referenceTrajectory = buildReferenceTrajectory(userTerminalMobility, slotStart, slotEnd);

    bool hasReferenceSample = false;
    for (const auto& sample : referenceTrajectory) {
        if (sample.reachable) {
            hasReferenceSample = true;
            break;
        }
    }
    if (!hasReferenceSample)
        return -1;

    int bestSatellite = -1;
    double bestScore = std::numeric_limits<double>::infinity();
    double bestCurrentElevation = -std::numeric_limits<double>::infinity();

    int bestReachableSatellite = -1;
    double bestReachableScore = std::numeric_limits<double>::infinity();
    double bestReachableElevation = -std::numeric_limits<double>::infinity();

    for (int satNum = 0; satNum < numOfSats; satNum++) {
        const auto candidateTrajectory = buildCandidateTrajectory(satNum, userTerminalMobility, slotStart, slotEnd);
        bool hasCandidateSample = false;
        for (const auto& sample : candidateTrajectory) {
            if (sample.reachable) {
                hasCandidateSample = true;
                break;
            }
        }
        if (!hasCandidateSample)
            continue;

        const double score = computeDtwDistance(referenceTrajectory, candidateTrajectory);
        const TopocentricSample& firstSample = candidateTrajectory.front();
        const double currentElevation = firstSample.reachable ? firstSample.elevation : -std::numeric_limits<double>::infinity();

        if (score < bestScore || (score == bestScore && currentElevation > bestCurrentElevation)) {
            bestScore = score;
            bestCurrentElevation = currentElevation;
            bestSatellite = satNum;
        }

        if (firstSample.reachable && (score < bestReachableScore || (score == bestReachableScore && currentElevation > bestReachableElevation))) {
            bestReachableScore = score;
            bestReachableElevation = currentElevation;
            bestReachableSatellite = satNum;
        }
    }

    // During handover we need a satellite that can carry traffic now; otherwise
    // the reconnect path may ignore an immediately usable satellite in favor of
    // one that only becomes reachable later in the lookahead window.
    if (bestReachableSatellite >= 0)
        return bestReachableSatellite;

    return bestSatellite;
}

std::vector<LeoChannelConstructor::TopocentricSample> LeoChannelConstructor::buildReferenceTrajectory(const GroundStationMobility *mobility, simtime_t slotStart, simtime_t slotEnd) const
{
    std::vector<TopocentricSample> trajectory;
    if (mobility == nullptr)
        return trajectory;

    const double latitude = mobility->getLUTPositionY();
    const double longitude = mobility->getLUTPositionX();
    const simtime_t sampleStep = userTerminalSampleInterval > SIMTIME_ZERO ? userTerminalSampleInterval : userTerminalUpdateInterval;

    for (simtime_t sampleTime = slotStart; sampleTime <= slotEnd; sampleTime += sampleStep) {
        TopocentricSample bestSample;
        bestSample.elevation = -std::numeric_limits<double>::infinity();

        // We do not have an external "isolated trajectory" trace in the simulator,
        // so the slot reference is the strongest overhead path seen across visible satellites.
        for (int satNum = 0; satNum < numOfSats; satNum++) {
            INorad *noradModule = satelliteNoradModules[satNum];
            if (noradModule == nullptr || !noradModule->isReachableAtTime(sampleTime, latitude, longitude, 0))
                continue;

            const double elevation = noradModule->getElevationAtTime(sampleTime, latitude, longitude, 0);
            if (!bestSample.reachable || elevation > bestSample.elevation) {
                bestSample.reachable = true;
                bestSample.elevation = elevation;
                bestSample.azimuth = noradModule->getAzimuthAtTime(sampleTime, latitude, longitude, 0);
            }
        }

        trajectory.push_back(bestSample);
        if (sampleStep == SIMTIME_ZERO)
            break;
    }

    return trajectory;
}

std::vector<LeoChannelConstructor::TopocentricSample> LeoChannelConstructor::buildCandidateTrajectory(int satNum, const GroundStationMobility *mobility, simtime_t slotStart, simtime_t slotEnd) const
{
    std::vector<TopocentricSample> trajectory;
    if (mobility == nullptr || satNum < 0 || satNum >= numOfSats)
        return trajectory;

    INorad *noradModule = satelliteNoradModules[satNum];
    if (noradModule == nullptr)
        return trajectory;

    const double latitude = mobility->getLUTPositionY();
    const double longitude = mobility->getLUTPositionX();
    const simtime_t sampleStep = userTerminalSampleInterval > SIMTIME_ZERO ? userTerminalSampleInterval : userTerminalUpdateInterval;

    for (simtime_t sampleTime = slotStart; sampleTime <= slotEnd; sampleTime += sampleStep) {
        TopocentricSample sample;
        sample.reachable = noradModule->isReachableAtTime(sampleTime, latitude, longitude, 0);
        if (sample.reachable) {
            sample.elevation = noradModule->getElevationAtTime(sampleTime, latitude, longitude, 0);
            sample.azimuth = noradModule->getAzimuthAtTime(sampleTime, latitude, longitude, 0);
        }
        trajectory.push_back(sample);
        if (sampleStep == SIMTIME_ZERO)
            break;
    }

    return trajectory;
}

double LeoChannelConstructor::computeDtwDistance(const std::vector<TopocentricSample>& referenceTrajectory, const std::vector<TopocentricSample>& candidateTrajectory) const
{
    const size_t referenceSize = referenceTrajectory.size();
    const size_t candidateSize = candidateTrajectory.size();
    if (referenceSize == 0 || candidateSize == 0)
        return std::numeric_limits<double>::infinity();

    constexpr double unreachablePenalty = 10000.0;
    std::vector<std::vector<double>> dtw(referenceSize + 1, std::vector<double>(candidateSize + 1, std::numeric_limits<double>::infinity()));
    dtw[0][0] = 0;

    for (size_t i = 1; i <= referenceSize; i++) {
        for (size_t j = 1; j <= candidateSize; j++) {
            const TopocentricSample& referenceSample = referenceTrajectory[i - 1];
            const TopocentricSample& candidateSample = candidateTrajectory[j - 1];

            double localCost = 0;
            if (referenceSample.reachable && candidateSample.reachable) {
                const double azimuthDelta = std::fabs(referenceSample.azimuth - candidateSample.azimuth);
                const double wrappedAzimuthDelta = std::min(azimuthDelta, 360.0 - azimuthDelta);
                localCost = wrappedAzimuthDelta + std::fabs(referenceSample.elevation - candidateSample.elevation);
            }
            else if (referenceSample.reachable != candidateSample.reachable) {
                localCost = unreachablePenalty;
            }

            dtw[i][j] = localCost + std::min({dtw[i - 1][j], dtw[i][j - 1], dtw[i - 1][j - 1]});
        }
    }

    return dtw[referenceSize][candidateSize];
}

void LeoChannelConstructor::createUserTerminalLink(int userTerminalIndex, int satNum)
{
    cModule *satModule = satelliteModules[satNum];
    cModule *userTerminalModule = userTerminalModules[userTerminalIndex];
    if (satModule == nullptr || userTerminalModule == nullptr)
        return;

    auto satGatePair = getNextFreeGate(satModule);
    auto userTerminalGatePair = getNextFreeGate(userTerminalModule);

    cGate *inGateSat = satGatePair.first;
    cGate *outGateSat = satGatePair.second;
    cGate *inGateUserTerminal = userTerminalGatePair.first;
    cGate *outGateUserTerminal = userTerminalGatePair.second;

    createChannel(0, outGateSat, inGateUserTerminal, false);
    createChannel(0, outGateUserTerminal, inGateSat, false);

    ActiveUserTerminalLink activeLink;
    activeLink.satelliteOutputGate = outGateSat;
    activeLink.userTerminalOutputGate = outGateUserTerminal;
    activeLink.satelliteMobility = satelliteMobilities[satNum];
    activeLink.userTerminalMobility = userTerminalMobilities[userTerminalIndex];
    activeLink.forwardChannel = check_and_cast<cDatarateChannel *>(outGateSat->getChannel());
    activeLink.reverseChannel = check_and_cast<cDatarateChannel *>(outGateUserTerminal->getChannel());
    activeLink.satelliteIndex = satNum;
    activeLink.userTerminalIndex = userTerminalIndex;

    dirtyPppModules.insert(satModule);
    dirtyPppModules.insert(userTerminalModule);
    activeUserTerminalLinks[userTerminalIndex] = activeLink;
}

void LeoChannelConstructor::removeUserTerminalLink(int userTerminalIndex, ActiveUserTerminalLink& link)
{
    cModule *satModule = satelliteModules[link.satelliteIndex];
    cModule *userTerminalModule = userTerminalModules[userTerminalIndex];
    if (satModule == nullptr || userTerminalModule == nullptr)
        return;

    const int satelliteGateIndex = link.satelliteOutputGate->getIndex();
    const int userTerminalGateIndex = link.userTerminalOutputGate->getIndex();

    NetworkInterface *userTerminalInterface = dynamic_cast<NetworkInterface *>(userTerminalModule->getSubmodule("ppp", userTerminalGateIndex));
    NetworkInterface *satelliteInterface = dynamic_cast<NetworkInterface *>(satModule->getSubmodule("ppp", satelliteGateIndex));

    if (userTerminalInterface != nullptr && userTerminalInterface->getIpv4Address().getInt() != 0)
        configurator->eraseIpAddressMap(userTerminalInterface->getIpv4Address().getInt());
    if (satelliteInterface != nullptr && satelliteInterface->getIpv4Address().getInt() != 0)
        configurator->eraseIpAddressMap(satelliteInterface->getIpv4Address().getInt());

    link.userTerminalOutputGate->disconnect();
    link.satelliteOutputGate->disconnect();

    if (userTerminalInterface != nullptr)
        processLifecycleCommand(userTerminalInterface, "Crash");
    if (satelliteInterface != nullptr)
        processLifecycleCommand(satelliteInterface, "Crash");
}

void LeoChannelConstructor::finalizeUserTerminalLinks()
{
    for (const auto& [userTerminalIndex, activeLink] : activeUserTerminalLinks) {
        cModule *satModule = satelliteModules[activeLink.satelliteIndex];
        cModule *userTerminalModule = userTerminalModules[userTerminalIndex];
        if (satModule == nullptr || userTerminalModule == nullptr)
            continue;

        cGate *satelliteInputGate = satModule->gateHalf("pppg", cGate::INPUT, activeLink.satelliteOutputGate->getIndex());
        cGate *userTerminalInputGate = userTerminalModule->gateHalf("pppg", cGate::INPUT, activeLink.userTerminalOutputGate->getIndex());

        IInterfaceTable *satelliteIft = check_and_cast<IInterfaceTable *>(satModule->getSubmodule("interfaceTable"));
        if (NetworkInterface *satelliteInterface = satelliteIft->findInterfaceByNodeInputGateId(satelliteInputGate->getId())) {
            processLifecycleCommand(satelliteInterface, "Start");
            configurator->addIpAddressMap(satelliteInterface->getIpv4Address().getInt(), satModule->getFullName());
        }

        IInterfaceTable *userTerminalIft = check_and_cast<IInterfaceTable *>(userTerminalModule->getSubmodule("interfaceTable"));
        if (NetworkInterface *userTerminalInterface = userTerminalIft->findInterfaceByNodeInputGateId(userTerminalInputGate->getId())) {
            processLifecycleCommand(userTerminalInterface, "Start");
            configurator->addIpAddressMap(userTerminalInterface->getIpv4Address().getInt(), userTerminalModule->getFullName());
        }
    }

    configurator->setGroundStationsWithEndpoints();
}


void LeoChannelConstructor::updatePPPModules(cModule *mod, bool addToGraph)
{
    const std::string moduleTypeName = mod->getNedTypeName();
    cModuleType *mainPppModuleType;
    if(!addToGraph && !isUserTerminalTypeName(moduleTypeName)){
        mainPppModuleType = cModuleType::get("leosatellites.linklayer.ppp.PppInterfaceMutable");
    }
    else{
        mainPppModuleType = cModuleType::get(interfaceType);
    }

    int existingPppVectorSize = 0;
    if (cModule *existingPpp0 = mod->getSubmodule("ppp", 0))
        existingPppVectorSize = existingPpp0->getVectorSize();
    int submoduleVectorSize = std::max(mod->gateSize("pppg"), existingPppVectorSize);
    mod->setSubmoduleVectorSize("ppp", submoduleVectorSize);
    cModule *module = nullptr;
    for(int i = 0; i < submoduleVectorSize; i++){
        cModuleType *pppModuleType = mainPppModuleType;
        if(!mod->getSubmodule("ppp", i)){
            cGate *srcGateOut = mod->gateHalf("pppg", cGate::OUTPUT, i);  //ADD BACK WITH RELEVANT CODE AT SOME POINT
            cGate *srcGateIn = mod->gateHalf("pppg", cGate::INPUT, i);

            cModule* destMod;

            std::string gateOwnerName = srcGateOut->getPathEndGate()->getBaseName();
            if(gateOwnerName == "pppg"){
                destMod = srcGateOut->getPathEndGate()->getOwnerModule();
            }
            else{
                destMod = srcGateOut->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
            }

            const std::string destTypeName = destMod->getNedTypeName();
            const bool usesLegacyEndUserInterface = isEndUserTypeName(moduleTypeName) || isEndUserTypeName(destTypeName);
            const bool usesUserTerminalInterface = isUserTerminalTypeName(moduleTypeName) || isUserTerminalTypeName(destTypeName);
            if(usesLegacyEndUserInterface && !usesUserTerminalInterface){
                pppModuleType = cModuleType::get("leosatellites.linklayer.ppp.PppInterfaceMutable");
            }

            module = pppModuleType->create("ppp", mod, i);
            //module->getSubmodule("queue")->par("packetCapacity") = 102;

            cChannelType *idealChannelType = cChannelType::get("ned.IdealChannel");
            cChannel *idealChannel = idealChannelType->create("idealChannel");
            cChannel *idealChannel2 = idealChannelType->create("idealChannel");

            cModule *nlModule = mod->getSubmodule("nl");
            cGate *upLayerInGate = module->gate("upperLayerIn");
            cGate *upLayerOutGate = module->gate("upperLayerOut");

            cGate *physInGate = module->gate("phys$i");
            cGate *physOutGate = module->gate("phys$o");

            int nlOutGateSize = nlModule->gateSize("out");
            int nlInGateSize = nlModule->gateSize("in");
            nlModule->setGateSize("out", nlOutGateSize+1);
            nlModule->setGateSize("in", nlInGateSize+1);

            cGate *nlOutGate = nlModule->gate("out", nlOutGateSize);
            cGate *nlInGate = nlModule->gate("in", nlInGateSize);

            nlOutGate->connectTo(upLayerInGate);
            upLayerOutGate->connectTo(nlInGate);

            physOutGate->connectTo(srcGateOut, idealChannel);
            srcGateIn->connectTo(physInGate, idealChannel2);

            //NetworkInterface* pppInterfaceModule = dynamic_cast<NetworkInterface*>(module);
            //pppInterfaceModule->addSubmodule(type, name, index)
            //cModuleType *moduleType = cModuleType::get("inet.queueing.queue.DropTailQueue");
            //cModule *pppInterfaceMod = moduleType->create("queue", pppInterfaceModule);

            module->finalizeParameters();
            module->buildInside();
            std::string queueSizeString = std::to_string(queueSize);
            module->getSubmodule("queue")->par("packetCapacity").parse(queueSizeString.c_str());
            module->scheduleStart(simTime());
            module->callInitialize();  //error here - trying to initisalise already existing module.

            NetworkInterface* ie = dynamic_cast<NetworkInterface*>(mod->getSubmodule("ppp", i));
            setUpIpLayer(module, ie);
            //std::cout << "\n GATE IN NODE: " << srcGateOut->getPathEndGate()->getOwner()->getOwner()->getOwner()->getFullName() << endl;
            //std::cout << "\n GATE OUT NODE: " << mod->getFullName() << endl;

            NetworkInterface* die = dynamic_cast<NetworkInterface*>(destMod->getSubmodule("ppp", i));
            if(addToGraph){
                configurator->addIpAddressMap(ie->getIpv4Address().getInt(), mod->getFullName());
                configurator->addNextHopInterface(mod, destMod, ie->getInterfaceId());
            }
//            else{
//                configurator->addIpv4NextHop(mod, die->getIpv4Address().getInt(), ie->getInterfaceId());
//            }
            //add
            //configurator->assignAddress(InterfaceEntry1)
            //node->configureInterface(InterfaceEntry1)
            // configure routing table?
        }
    }
}

void LeoChannelConstructor::setUpIpLayer(cModule* module, NetworkInterface* interface)
{
    Ipv4Address address = Ipv4Address(addressBase.getInt() + uint32_t(module->getId()));
    //configurator->assignNewAddress(module);

    prepareInterface(interface);
    Ipv4InterfaceData *interfaceData = interface->getProtocolDataForUpdate<Ipv4InterfaceData>();
    interfaceData->setIPAddress(address);
    interfaceData->setNetmask(netmask);
    interface->setBroadcast(true);

    interfaceData->joinMulticastGroup(Ipv4Address::ALL_HOSTS_MCAST);
    interfaceData->joinMulticastGroup(Ipv4Address::ALL_ROUTERS_MCAST);

}

void LeoChannelConstructor::prepareInterface(NetworkInterface *interfaceEntry)
{
    Ipv4InterfaceData *interfaceData = interfaceEntry->addProtocolData<Ipv4InterfaceData>();
    auto datarate = interfaceEntry->getDatarate();
    // TODO: KLUDGE: how do we set the metric correctly for both wired and wireless interfaces even if datarate is unknown
    if (datarate == 0)
        interfaceData->setMetric(1);
    else
        // metric: some hints: OSPF cost (2e9/bps value), MS KB article Q299540, ...
        interfaceData->setMetric((int)ceil(2e9 / datarate));    // use OSPF cost as default
    if (interfaceEntry->isMulticast()) {
        interfaceData->joinMulticastGroup(Ipv4Address::ALL_HOSTS_MCAST);
        interfaceData->joinMulticastGroup(Ipv4Address::ALL_ROUTERS_MCAST);
    }
}

uint64_t LeoChannelConstructor::getGroundStationLinkKey(int gsNum, int satNum) const
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(gsNum)) << 32) | static_cast<uint32_t>(satNum);
}

void LeoChannelConstructor::updateTimedLink(const TimedLinkRecord& linkRecord)
{
    double delaySeconds = 0;
    if (linkRecord.isGroundStationLink) {
        delaySeconds = (linkRecord.sourceSatelliteMobility->getDistance(linkRecord.destinationGroundStationMobility->getLUTPositionY(),
                                                                       linkRecord.destinationGroundStationMobility->getLUTPositionX(),
                                                                       0) * 1000.0) / 299792458.0;
        configurator->addGSLinktoTopologyGraph(numOfSats + linkRecord.groundStationIndex, linkRecord.satelliteIndex, delaySeconds * 1000.0);
    }
    else {
        delaySeconds = (linkRecord.sourceSatelliteMobility->getDistance(linkRecord.destinationSatelliteMobility->getLatitude(),
                                                                       linkRecord.destinationSatelliteMobility->getLongitude(),
                                                                       linkRecord.destinationSatelliteMobility->getAltitude()) * 1000.0) / 299792458.0;
    }

    linkRecord.forwardChannel->setDelay(delaySeconds);
    linkRecord.forwardChannel->setDatarate(linkDataRate);
    linkRecord.reverseChannel->setDelay(delaySeconds);
    linkRecord.reverseChannel->setDatarate(linkDataRate);
}

void LeoChannelConstructor::createGroundStationLink(int gsNum, int satNum)
{
    cModule *satMod = satelliteModules[satNum];
    cModule *gsMod = groundStationModules[gsNum];

    auto satGatePair = getNextFreeGate(satMod);
    auto gsGatePair = getNextFreeGate(gsMod);

    cGate *inGateSat = satGatePair.first;
    cGate *outGateSat = satGatePair.second;
    cGate *inGateGs = gsGatePair.first;
    cGate *outGateGs = gsGatePair.second;

    createChannel(0, outGateSat, inGateGs, false);
    createChannel(0, outGateGs, inGateSat, false);

    auto *forwardChannel = check_and_cast<cDatarateChannel *>(outGateSat->getChannel());
    auto *reverseChannel = check_and_cast<cDatarateChannel *>(outGateGs->getChannel());

    ActiveGroundStationLink activeLink;
    activeLink.satelliteOutputGate = outGateSat;
    activeLink.groundStationOutputGate = outGateGs;
    activeLink.linkRecord = {satelliteMobilities[satNum], nullptr, groundStationMobilities[gsNum],
                             forwardChannel, reverseChannel, satNum, gsNum, true};

    IInterfaceTable* sourceIft = check_and_cast<IInterfaceTable*>(satMod->getSubmodule("interfaceTable"));
    if (NetworkInterface* ie = sourceIft->findInterfaceByNodeInputGateId(inGateSat->getId())) {
        processLifecycleCommand(ie, "Start");
        configurator->addNextHopInterface(satMod, gsMod, ie->getInterfaceId());
        configurator->addIpAddressMap(ie->getIpv4Address().getInt(), satMod->getFullName());
    }

    IInterfaceTable* destIft = check_and_cast<IInterfaceTable*>(gsMod->getSubmodule("interfaceTable"));
    if (NetworkInterface* die = destIft->findInterfaceByNodeInputGateId(inGateGs->getId())) {
        processLifecycleCommand(die, "Start");
        configurator->addNextHopInterface(gsMod, satMod, die->getInterfaceId());
        configurator->addIpAddressMap(die->getIpv4Address().getInt(), gsMod->getFullName());
    }

    dirtyPppModules.insert(satMod);
    dirtyPppModules.insert(gsMod);
    activeGroundStationLinks.emplace(getGroundStationLinkKey(gsNum, satNum), activeLink);
}

void LeoChannelConstructor::removeGroundStationLink(int gsNum, int satNum, ActiveGroundStationLink& link)
{
    cModule *satMod = satelliteModules[satNum];
    cModule *gsMod = groundStationModules[gsNum];

    configurator->removeNextHopInterface(gsMod, satMod);
    configurator->removeNextHopInterface(satMod, gsMod);

    const int gsGateIndex = link.groundStationOutputGate->getIndex();
    const int satGateIndex = link.satelliteOutputGate->getIndex();

    NetworkInterface *gsInterface = dynamic_cast<NetworkInterface *>(gsMod->getSubmodule("ppp", gsGateIndex));
    NetworkInterface *satInterface = dynamic_cast<NetworkInterface *>(satMod->getSubmodule("ppp", satGateIndex));

    if (gsInterface != nullptr && gsInterface->getIpv4Address().getInt() != 0)
        configurator->eraseIpAddressMap(gsInterface->getIpv4Address().getInt());
    if (satInterface != nullptr && satInterface->getIpv4Address().getInt() != 0)
        configurator->eraseIpAddressMap(satInterface->getIpv4Address().getInt());

    link.groundStationOutputGate->disconnect();
    link.satelliteOutputGate->disconnect();

    if (gsInterface != nullptr)
        processLifecycleCommand(gsInterface, "Crash");
    if (satInterface != nullptr)
        processLifecycleCommand(satInterface, "Crash");
}

void LeoChannelConstructor::createChannel(double delaySeconds, cGate *gate1, cGate*gate2, bool endPointChannel)
{
    cChannelType *channelType = cChannelType::get("ned.DatarateChannel");
    auto *channel = check_and_cast<cDatarateChannel *>(channelType->create("channel"));
    channel->setDelay(delaySeconds);
    channel->setDatarate(endPointChannel ? 1e12 : linkDataRate);
    gate1->connectTo(gate2, channel);
}

void LeoChannelConstructor::processLifecycleCommand(cModule *module, std::string command)
{
    LifecycleOperation *operation;

    if (command == "Start") {
        operation = new ModuleStartOperation();
    }
    else if (command == "Stop") {
        operation = new ModuleStopOperation();
    }
    else if (command == "Crash") {
        operation = new ModuleCrashOperation();
    }
    else {
        operation = check_and_cast<LifecycleOperation *>(inet::utils::createOne(command.c_str()));
    }

    cXMLAttributeMap* emptyParam;
    std::map<basic_string<char>, basic_string<char>> emptyMap;
    operation->initialize(module, emptyMap);

    initiateOperation(operation);
}

void LeoChannelConstructor::finish()
{

}

}
