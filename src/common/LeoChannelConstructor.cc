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
#include <cstring>
#include <inet/common/INETUtils.h>
#include <inet/common/XMLUtils.h>
#include <inet/common/ModuleAccess.h>
#include <inet/common/lifecycle/LifecycleOperation.h>
#include <inet/common/lifecycle/ModuleOperations.h>

#include "LeoChannelConstructor.h"

using namespace std;
namespace inet {
Define_Module(LeoChannelConstructor);

LeoChannelConstructor::~LeoChannelConstructor() {
    cancelAndDelete(startManagerNode);
    cancelAndDelete(updateTimer);
}

LeoChannelConstructor::LeoChannelConstructor(){
    numOfPlanes = 0;
    satPerPlane = 0;
    startManagerNode = nullptr;
    updateTimer = nullptr;
    updateInterval = 0;
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

        // Get reference to the parent module
        cModule *parent = getParentModule();

        // Retrieve satellite network parameters from parent module
        int planes = parent->par("numOfPlanes");
        numOfSats = parent->par("numOfSats");
        numOfGS = parent->par("numOfGS");
        numOfClients = parent->par("numOfClients");
        satPerPlane = parent->par("satsPerPlane");
        enableInterSatelliteLinks = parent->par("enableInterSatelliteLinks").boolValue();

        // Calculate total number of planes, accounting for unfilled ones
        numOfPlanes = (int)std::ceil(((double)numOfSats / ((double)planes * (double)satPerPlane)) * (double)planes);

        // Get reference to the configurator submodule
        configurator = dynamic_cast<LeoIpv4NetworkConfigurator*>(parent->getSubmodule("configurator"));

        // Initialize update and interval counters
        updateInterval = 0;
        currentInterval = 0;

        // Get general network configuration
        networkName = parent->getName();
        linkDataRate = par("dataRate").str();
        queueSize = par("queueSize");
        interfaceType = par("interfaceType").stringValue();

        // Start manager node at simulation time 0
        scheduleAt(0, startManagerNode);
    }
}

//TODO move the message handling to configurator?
void LeoChannelConstructor::handleMessage(cMessage *msg)
{
    if(msg == updateTimer){
        setUpGSLinks();
        addPPPInterfaces();
        //setUpInterfaces();
        updateChannels();
        configurator->updateForwardingStates(simTime());

        //update all routing tables
        //perform global arp

        //configurator->configure();
        scheduleUpdate(false);
    }
    else if(msg == startManagerNode){
        setUpSimulation();
        setUpGSLinks();

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
    //Create Client links
    cGate *inGateClient;
    cGate *outGateClient;
    cGate *inGateGs;
    cGate *outGateGs;
    std::vector<std::string> endUserTypes = {"client", "server"};
    for (const auto& type : endUserTypes) {
        for (int clientNum = 0; clientNum < numOfClients; clientNum++) {
            std::string moduleName = networkName + "." + type + "[" + std::to_string(clientNum) + "]";
            cModule *module = getModuleByPath(moduleName.c_str());

            std::string connectName = module->par("connectModule");
            std::string destFullName = networkName + "." + connectName;
            cModule *destMod = getModuleByPath(destFullName.c_str());

            std::pair<cGate*, cGate*> gatePair1 = getNextFreeGate(module);
            std::pair<cGate*, cGate*> gatePair2 = getNextFreeGate(destMod);

            cGate* inGate = gatePair1.first;
            cGate* outGate = gatePair1.second;
            cGate* inGateDest = gatePair2.first;
            cGate* outGateDest = gatePair2.second;

            std::string dString = "0ms";
            createChannel(dString, outGate, inGateDest, true);
            createChannel(dString, outGateDest, inGate, true);
        }
    }

    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        if(satNum == 0){
            updateInterval = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->par("updateInterval").doubleValue();
        }
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
            std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
            cModule *satMod = getModuleByPath(satName.c_str()); // get source satellite module

            cGate *inGateSat1;
            cGate *outGateSat1;
            cGate *inGateSat2;
            cGate *outGateSat2;
            int destSatNumA = (satNum+1)%(satPerPlane*(planeNum+1));
            if(destSatNumA == 0){
                destSatNumA = planeNum*satPerPlane; //If number is zero, must be start of orbital plane
            }

            if(destSatNumA < numOfSats){
                std::string destSatNameA = std::string(networkName + ".satellite[" + std::to_string(destSatNumA) + "]");  //+1 within same orbital plane ISL
                cModule *destModA = getModuleByPath(destSatNameA.c_str());
                std::pair <cGate*, cGate*> gatePair1 = getNextFreeGate(satMod);
                std::pair <cGate*, cGate*> gatePair2 = getNextFreeGate(destModA);

                inGateSat1 = gatePair1.first;
                outGateSat1 = gatePair1.second;
                inGateSat2 = gatePair2.first;
                outGateSat2 = gatePair2.second;

                //cChannel *channel = channelType->create("channel");
                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModA->getSubmodule("mobility"));
                double distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude())*1000;
                std::string dString = std::to_string((distance/299792458)*1000) + "ms";

                createChannel(dString, outGateSat1, inGateSat2, false);
                createChannel(dString, outGateSat2, inGateSat1, false);
            }

            int destSatNumB = (satNum + satPerPlane);// % totalSats;
            if(destSatNumB < numOfSats){
                std::string destSatNameB = std::string(networkName + ".satellite[" + std::to_string(destSatNumB) + "]"); //+1 adjacent orbital plane ISL
                cModule *destModB = getModuleByPath(destSatNameB.c_str());
                std::pair <cGate*, cGate*> gatePair1 = getNextFreeGate(satMod);
                std::pair <cGate*, cGate*> gatePair2 = getNextFreeGate(destModB);

                inGateSat1 = gatePair1.first;
                outGateSat1 = gatePair1.second;
                inGateSat2 = gatePair2.first;
                outGateSat2 = gatePair2.second;

                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModB->getSubmodule("mobility"));
                double distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude())*1000;
                std::string dString = std::to_string((distance/299792458)*1000) + "ms";
                createChannel(dString, outGateSat1, inGateSat2, false);
                createChannel(dString, outGateSat2, inGateSat1, false);
            }
        }
    }
}

void LeoChannelConstructor::setUpClientServerInterfaces()
{
    for(int clientNum = 0; clientNum < numOfClients; clientNum++){
        std::string clientName = std::string(networkName + ".client[" + std::to_string(clientNum) + "]");
        cModule *clientMod = getModuleByPath(clientName.c_str());
        NetworkInterface* clientInterface = dynamic_cast<NetworkInterface*>(clientMod->getSubmodule("ppp", 0));
        setUpIpLayer(clientMod, clientInterface);
        dynamic_cast<LeoIpv4RoutingTable*>(clientMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();

        std::string serverName = std::string(networkName + ".server[" + std::to_string(clientNum) + "]");
        cModule *serverMod = getModuleByPath(serverName.c_str());
        NetworkInterface* serverInterface = dynamic_cast<NetworkInterface*>(serverMod->getSubmodule("ppp", 0));
        setUpIpLayer(serverMod, serverInterface);
        dynamic_cast<LeoIpv4RoutingTable*>(serverMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }
}

void LeoChannelConstructor::setUpInterfaces()
{
    std::vector<std::string> nodeTypes = {"client", "server"};

    for (const auto& type : nodeTypes) {
        for (int clientNum = 0; clientNum < numOfClients; clientNum++) {
            std::string moduleName = networkName + "." + type + "[" + std::to_string(clientNum) + "]";
            cModule *mod = getModuleByPath(moduleName.c_str());
            updatePPPModules(mod, false);
            auto* routingTable = dynamic_cast<LeoIpv4RoutingTable*>(mod->getModuleByPath(".ipv4.routingTable"));
            if (routingTable)
                routingTable->configureRouterId();
        }
    }

    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        updatePPPModules(satMod, true);
        dynamic_cast<LeoIpv4RoutingTable*>(satMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }

    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        std::string gsName = std::string(networkName + ".groundStation[" + std::to_string(gsNum) + "]");
        cModule *gsMod = getModuleByPath(gsName.c_str());
        updatePPPModules(gsMod, true);
        dynamic_cast<LeoIpv4RoutingTable*>(gsMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }
}

void LeoChannelConstructor::addPPPInterfaces(){
    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        updatePPPModules(satMod, true);
    }

    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        std::string gsName = std::string(networkName + ".groundStation[" + std::to_string(gsNum) + "]");
        cModule *gsMod = getModuleByPath(gsName.c_str());
        updatePPPModules(gsMod, true);
    }
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
    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        for(int i = 0; i < satMod->gateSize("pppg$o"); i++){  //check each possible pppg gate
            cGate* srcGate = satMod->gate("pppg$o", i);
            cGate* srcGate2 = satMod->gate("pppg$i", i);
            if(srcGate->isConnected()){
                cChannel *chan = srcGate->getChannel();
                cChannel *chan2 = srcGate2->getIncomingTransmissionChannel();
                cModule* destModule = srcGate->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
                std::string mobilityName = destModule->getSubmodule("mobility")->getNedTypeName();
                //std::string mobilityName = destModule->getModuleByPath("mobility")->getNedTypeName();
                double distance = 0;
                if(mobilityName == "leosatellites.mobility.SatelliteMobility"){ //Satellite to Satellite Channel
                    SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModule->getSubmodule("mobility"));
                    distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude())*1000;
                    distance = (distance/299792458)*1000; //ms
                }
                else if (mobilityName == "leosatellites.mobility.GroundStationMobility"){ // Satellite to Ground Station Channel
                    GroundStationMobility* destGSMobility = dynamic_cast<GroundStationMobility*>(destModule->getSubmodule("mobility"));
                    distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destGSMobility->getLUTPositionY(), destGSMobility->getLUTPositionX(), 0)*1000;
                    distance = (distance/299792458)*1000; //ms
                    configurator->addGSLinktoTopologyGraph(configurator->getNodeModuleGraphId(destModule->getFullName()), satNum, distance); //TODO FIX
                }
                else{
                    throw cRuntimeError("Unsupported mobility used by module");
                }

                std::string dString = std::to_string(distance) + "ms";

                cPar& param = chan->par("delay");
                param.parse(dString.c_str());
                chan->par("datarate").parse(linkDataRate.c_str());

                cPar& param2 = chan2->par("delay");
                param2.parse(dString.c_str());
                chan2->par("datarate").parse(linkDataRate.c_str());
            }
        }
    }
}

void LeoChannelConstructor::setUpGSLinks()
{
    //Sets up new GS links if new satellite is reachable.
    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        std::string gsName = std::string(networkName + ".groundStation[" + std::to_string(gsNum) + "]");
        cModule *gsMod = getModuleByPath(gsName.c_str());
        GroundStationMobility* gsMobility = dynamic_cast<GroundStationMobility*>(gsMod->getSubmodule("mobility"));
        for(int satNum = 0; satNum < numOfSats; satNum++){
            std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
            cModule *satMod = getModuleByPath(satName.c_str());
            SatelliteMobility* satMobility = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"));
            if(satMobility->isReachable(gsMobility->getLUTPositionY(), gsMobility->getLUTPositionX(), 0)){
                int gsGateSize = gsMod->gateSize("pppg");
                if(gsGateSize > 0){
                    bool linkExists = false;
                    for(int i = 0; i < gsGateSize; i++){
                        if((gsMod->gate("pppg$o", i)->getPathEndGate()->getOwnerModule()->getOwner()->getOwner() == satMod)){
                            //link already exists - do not do anything
                            linkExists = true;
                            goto setup;
                        }
                    }
                    setup:
                        if(!linkExists){
                            cGate *inGateSat;
                            cGate *outGateSat;
                            cGate *inGateGS;
                            cGate *outGateGS;
                            std::pair <cGate*, cGate*> gatePair1 = getNextFreeGate(satMod);
                            std::pair <cGate*, cGate*> gatePair2 = getNextFreeGate(gsMod);
                            inGateSat = gatePair1.first;
                            outGateSat = gatePair1.second;
                            inGateGS = gatePair2.first;
                            outGateGS = gatePair2.second;
                            cChannelType *channelType = cChannelType::get("ned.DatarateChannel");
                            cChannel *channel = channelType->create("channel");
                            cChannel *channel2 = channelType->create("channel");
                            outGateSat->connectTo(inGateGS, channel);
                            outGateGS->connectTo(inGateSat, channel2);
                            IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(satMod->getSubmodule("interfaceTable"));
                            NetworkInterface* ie = sourceIft->findInterfaceByNodeInputGateId(inGateSat->getId());
                            if(ie){
                                processLifecycleCommand(ie, "Start");
                                configurator->addNextHopInterface(satMod, gsMod, ie->getInterfaceId());
                                configurator->addIpAddressMap(ie->getIpv4Address().getInt(), satMod->getFullName());
                            }

                            IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(gsMod->getSubmodule("interfaceTable"));
                            NetworkInterface* die = destIft->findInterfaceByNodeInputGateId(inGateGS->getId());
                            if(die){
                                processLifecycleCommand(die, "Start");
                                configurator->addNextHopInterface(gsMod, satMod, die->getInterfaceId());
                                configurator->addIpAddressMap(die->getIpv4Address().getInt(), gsMod->getFullName());
                            }
                        }
                }
                else{
                    cGate *inGateSat;
                    cGate *outGateSat;
                    cGate *inGateGS;
                    cGate *outGateGS;
                    std::pair <cGate*, cGate*> gatePair1 = getNextFreeGate(satMod);
                    std::pair <cGate*, cGate*> gatePair2 = getNextFreeGate(gsMod);
                    inGateSat = gatePair1.first;
                    outGateSat = gatePair1.second;
                    inGateGS = gatePair2.first;
                    outGateGS = gatePair2.second;
                    //cChannelType *channelType = cChannelType::get("ned.DatarateChannel");
                    //double distance = satMobility->getDistance(gsMobility->getLUTPositionY(), gsMobility->getLUTPositionX(), 0)*1000;
                    //std::string dString = std::to_string((distance/299792458)*1000) + "ms";

                    cChannelType *channelType = cChannelType::get("ned.DatarateChannel");
                    cChannel *channel = channelType->create("channel");
                    cChannel *channel2 = channelType->create("channel");
                    outGateSat->connectTo(inGateGS, channel);
                    outGateGS->connectTo(inGateSat, channel2);
                    IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(satMod->getSubmodule("interfaceTable"));
                    NetworkInterface* ie = sourceIft->findInterfaceByNodeInputGateId(inGateSat->getId());
                    if(ie){
                        processLifecycleCommand(ie, "Start");
                        configurator->addNextHopInterface(satMod, gsMod, ie->getInterfaceId());
                        configurator->addIpAddressMap(ie->getIpv4Address().getInt(), satMod->getFullName());
                    }

                    IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(gsMod->getSubmodule("interfaceTable"));
                    NetworkInterface* die = destIft->findInterfaceByNodeInputGateId(inGateGS->getId());
                    if(die){
                        processLifecycleCommand(die, "Start");
                        configurator->addNextHopInterface(gsMod, satMod, die->getInterfaceId());
                        configurator->addIpAddressMap(die->getIpv4Address().getInt(), gsMod->getFullName());
                    }
                }
            }
            else{ //remove link is exists
                int gsGateSize = gsMod->gateSize("pppg");
                if(gsGateSize > 0){
                    for(int i = 0; i < gsGateSize; i++){
                        cGate* endGate = gsMod->gate("pppg$o", i)->getPathEndGate();
                        if((endGate->getOwnerModule()->getOwner()->getOwner() == satMod)){
                            //IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(gsMod->getSubmodule("interfaceTable"));
                            //NetworkInterface* ie = sourceIft->findInterfaceByNodeInputGateId(gsMod->gate("pppg$o", i)->getId());

                            //IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(satMod->getSubmodule("interfaceTable"));
                            //NetworkInterface* die = destIft->findInterfaceByNodeInputGateId(endGate->getId());

                           // configurator->addNextHopInterface(gsMod, satMod, die->getInterfaceId());
                            configurator->removeNextHopInterface(gsMod, satMod);//, die->getInterfaceId());
                            gsMod->gate("pppg$o", i)->disconnect();

                            NetworkInterface* ie = dynamic_cast<NetworkInterface*>(gsMod->getSubmodule("ppp", i));
                            processLifecycleCommand(ie, "Crash");
                        }
                    }
                }
                int satGateSize = satMod->gateSize("pppg");
                if(satGateSize > 0){
                    for(int i = 0; i < satGateSize; i++){
                        cGate* endGate = satMod->gate("pppg$o", i)->getPathEndGate();
                        if((endGate->getOwnerModule()->getOwner()->getOwner() == gsMod)){
                            configurator->removeNextHopInterface(satMod, gsMod);
                            satMod->gate("pppg$o", i)->disconnect();

                            NetworkInterface* ie = dynamic_cast<NetworkInterface*>(satMod->getSubmodule("ppp", i));
                            processLifecycleCommand(ie, "Crash");
                        }
                    }
                }
            }
        }
    }
}


void LeoChannelConstructor::updatePPPModules(cModule *mod, bool addToGraph)
{
    cModuleType *pppModuleType;
    if(!addToGraph){
        pppModuleType = cModuleType::get("leosatellites.linklayer.ppp.PppInterfaceMutable");
    }
    else{
        pppModuleType = cModuleType::get(interfaceType);
    }

    int submoduleVectorSize = mod->gateSize("pppg");
    for (SubmoduleIterator it(mod); !it.end(); ++it) {
        cModule *submodule = *it;
        if (submodule->isVector() && submodule->isName("ppp")) {
            if (submoduleVectorSize < submodule->getVectorSize())
                submoduleVectorSize = submodule->getVectorSize();
        }
    }
    mod->setSubmoduleVectorSize("ppp", submoduleVectorSize);
    cModule *module = nullptr;
    for(int i = 0; i < submoduleVectorSize; i++){
        if(!mod->getSubmodule("ppp", i)){
            cGate *srcGateOut = mod->gateHalf("pppg", cGate::OUTPUT, i);  //ADD BACK WITH RELEVANT CODE AT SOME POINT
            cGate *srcGateIn = mod->gateHalf("pppg", cGate::INPUT, i);

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

            cModule* destMod;

            std::string gateOwnerName = srcGateOut->getPathEndGate()->getBaseName();
            if(gateOwnerName == "pppg"){
                destMod = srcGateOut->getPathEndGate()->getOwnerModule();
            }
            else{
                destMod = srcGateOut->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
            }
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

void LeoChannelConstructor::createChannel(std::string delay, cGate *gate1, cGate*gate2, bool endPointChannel)
{
    cChannelType *channelType = cChannelType::get("ned.DatarateChannel");
    cChannel *channel = channelType->create("channel");
    channel->par("delay").parse(delay.c_str());

    if(!endPointChannel){
        channel->par("datarate").parse(linkDataRate.c_str());
    }
    else{
        channel->par("datarate").parse("1000Gbps");
    }
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
