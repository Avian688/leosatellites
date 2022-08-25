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

#include "LeoChannelConstructor.h"
#include "inet/common/ModuleAccess.h"
#include <fstream> //check if needed
#include <cstring>

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
        addressBase = Ipv4Address(par("addressBase").stringValue());
        netmask = Ipv4Address(par("netmask").stringValue());
        startManagerNode = new cMessage("startManagerNode");
        updateTimer = new cMessage("update");
        int planes = getAncestorPar("numOfPlanes");
        numOfSats = getAncestorPar("numOfSats");
        numOfGS = getAncestorPar("numOfGS");
        satPerPlane = getAncestorPar("satsPerPlane");
        numOfPlanes = (int)std::ceil(((double)numOfSats/((double)planes*(double)satPerPlane))*(double)planes); //depending on number of satellites, all planes may not be filled
        configurator = dynamic_cast<LeoIpv4NetworkConfigurator*>(getParentModule()->getSubmodule("configurator"));
        updateInterval = 0;
        currentInterval = 0;
        networkName = getParentModule()->getName();
        linkDataRate = par("dataRate").str();
        scheduleAt(0, startManagerNode);

    }
}

//TODO move the message handling to configurator?
void LeoChannelConstructor::handleMessage(cMessage *msg)
{
    if(msg == updateTimer){
        currentInterval += updateInterval;
        //std::cout << "Updating at time: " << simTime() << endl;
        setUpGSLinks();
        addPPPInterfaces();
        //setUpInterfaces();
        updateChannels();
        configurator->updateForwardingStates(currentInterval);
        //configurator->generateTopologyGraph(currentInterval);

        //update all routing tables

        //perform global arp

        //configurator->configure();
        scheduleUpdate();
    }
    else if(msg == startManagerNode){
        //updateChannels();

        setUpSimulation();
        configurator->establishInitialISLs();
        configurator->updateForwardingStates(currentInterval);
        //configurator->generateTopologyGraph(currentInterval);
        scheduleUpdate();
    }
    else{
        throw cRuntimeError("Unknown message arrived at channel constructor.");
    }
}

void LeoChannelConstructor::scheduleUpdate()
{
    cancelEvent(updateTimer);
    simtime_t nextUpdate = simTime() + SimTime(updateInterval);//updateInterval;
    scheduleAt(nextUpdate, updateTimer);
}

void LeoChannelConstructor::setUpSimulation()
{
    //cChannelType *channelType = cChannelType::get("ned.DatarateChannel"); //replace with either gs channel or laser link channel ned file
    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        if(satNum == 0){
            updateInterval = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->par("updateInterval").doubleValue() + 0.000000001;
        }
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
                //configurator->addToISLMobilityMap(dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility")), destSatMobility);
                double distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude())*1000;
                std::string dString = std::to_string((distance/299792458)*1000) + "ms";

                createChannel(dString, outGateSat1, inGateSat2);
                createChannel(dString, outGateSat2, inGateSat1);
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
                //configurator->addToISLMobilityMap(dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility")), destSatMobility);
                double distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude())*1000;
                std::string dString = std::to_string((distance/299792458)*1000) + "ms";
                createChannel(dString, outGateSat1, inGateSat2);
                createChannel(dString, outGateSat2, inGateSat1);
            }
        }
    }

    setUpGSLinks();
    setUpInterfaces();
    updateChannels();
}

void LeoChannelConstructor::setUpInterfaces()
{
    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        updatePPPModules(satMod);
        dynamic_cast<LeoIpv4RoutingTable*>(satMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }

    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        std::string gsName = std::string(networkName + ".groundStation[" + std::to_string(gsNum) + "]");
        cModule *gsMod = getModuleByPath(gsName.c_str());
        updatePPPModules(gsMod);
        dynamic_cast<LeoIpv4RoutingTable*>(gsMod->getModuleByPath(".ipv4.routingTable"))->configureRouterId();
    }
}

void LeoChannelConstructor::addPPPInterfaces(){
    for(int satNum = 0; satNum < numOfSats; satNum++){
        std::string satName = std::string(networkName + ".satellite[" + std::to_string(satNum) + "]");
        cModule *satMod = getModuleByPath(satName.c_str());
        updatePPPModules(satMod);
    }

    for(int gsNum = 0; gsNum < numOfGS; gsNum++){
        std::string gsName = std::string(networkName + ".groundStation[" + std::to_string(gsNum) + "]");
        cModule *gsMod = getModuleByPath(gsName.c_str());
        updatePPPModules(gsMod);
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
    return std::make_pair(inGate, outGate);
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
                if(mobilityName == "leosatellites.mobility.SatelliteMobility"){
                    SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModule->getSubmodule("mobility"));
                    distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), destSatMobility->getAltitude())*1000;
                    distance = (distance/299792458)*1000; //ms
                }
                else if (mobilityName == "leosatellites.mobility.GroundStationMobility"){
                    GroundStationMobility* destGSMobility = dynamic_cast<GroundStationMobility*>(destModule->getSubmodule("mobility"));
                    distance = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"))->getDistance(destGSMobility->getLUTPositionY(), destGSMobility->getLUTPositionX(), 0)*1000;
                    distance = (distance/299792458)*1000; //ms
                    configurator->addGSLinktoTopologyGraph(destModule->getIndex(), satNum, distance);
                }
                else{
                    throw cRuntimeError("Unsupported mobility used by module");
                }

                cPar& param = chan->par("delay");
                std::string dString = std::to_string(distance) + "ms";
                param.parse(dString.c_str());
                chan->par("datarate").parse("10Mbps");

                cPar& param2 = chan2->par("delay");
                param2.parse(dString.c_str());
                chan2->par("datarate").parse("10Mbps");
            }
        }
    }
}

void LeoChannelConstructor::setUpGSLinks()
{
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
                            InterfaceEntry* ie = sourceIft->findInterfaceByNodeInputGateId(inGateSat->getId());
                            if(ie){
                                configurator->addNextHopInterface(satMod, gsMod, ie->getInterfaceId());
                            }

                            IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(gsMod->getSubmodule("interfaceTable"));
                            InterfaceEntry* die = destIft->findInterfaceByNodeInputGateId(inGateGS->getId());
                            if(die){
                                configurator->addNextHopInterface(gsMod, satMod, die->getInterfaceId());
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
                    InterfaceEntry* ie = sourceIft->findInterfaceByNodeInputGateId(inGateSat->getId());
                    if(ie){
                        configurator->addNextHopInterface(satMod, gsMod, ie->getInterfaceId());
                    }

                    IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(gsMod->getSubmodule("interfaceTable"));
                    InterfaceEntry* die = destIft->findInterfaceByNodeInputGateId(inGateGS->getId());
                    if(die){
                        configurator->addNextHopInterface(gsMod, satMod, die->getInterfaceId());
                    }
                }
            }
            else{ //remove link is exists
                int gsGateSize = gsMod->gateSize("pppg");
                if(gsGateSize > 0){
                    for(int i = 0; i < gsGateSize; i++){
                        cGate* endGate = gsMod->gate("pppg$o", i)->getPathEndGate();
                        if((endGate->getOwnerModule()->getOwner()->getOwner() == satMod)){
                            gsMod->gate("pppg$o", i)->disconnect();
                        }
                    }
                }
                int satGateSize = satMod->gateSize("pppg");
                if(satGateSize > 0){
                    for(int i = 0; i < satGateSize; i++){
                        cGate* endGate = satMod->gate("pppg$o", i)->getPathEndGate();
                        if((endGate->getOwnerModule()->getOwner()->getOwner() == gsMod)){
                            satMod->gate("pppg$o", i)->disconnect();
                        }
                    }
                }
            }
        }
    }
}


void LeoChannelConstructor::updatePPPModules(cModule *mod)
{
    cModuleType *pppModuleType = cModuleType::get("inet.linklayer.ppp.PppInterface");
    int submoduleVectorSize = mod->gateSize("pppg");
    for (SubmoduleIterator it(mod); !it.end(); ++it) {
        cModule *submodule = *it;
        if (submodule->isVector() && submodule->isName("ppp")) {
            if (submoduleVectorSize < submodule->getVectorSize())
                submoduleVectorSize = submodule->getVectorSize();
        }
    }

    cModule *module = nullptr;
    for(int i = 0; i < submoduleVectorSize; i++){
        if(!mod->getSubmodule("ppp", i)){
            cGate *srcGateOut = mod->gateHalf("pppg", cGate::OUTPUT, i);  //ADD BACK WITH RELEVANT CODE AT SOME POINT
            cGate *srcGateIn = mod->gateHalf("pppg", cGate::INPUT, i);

            module = pppModuleType->create("ppp", mod, submoduleVectorSize, i);
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
            module->finalizeParameters();
            module->buildInside();
            module->scheduleStart(simTime());

            module->callInitialize();  //error here - trying to initisalise already existing module.
            Ipv4Address address = Ipv4Address(addressBase.getInt() + uint32(module->getId()));

            //configurator->assignNewAddress(module);
            InterfaceEntry* ie = dynamic_cast<InterfaceEntry*>(mod->getSubmodule("ppp", i));
            prepareInterface(ie);
            Ipv4InterfaceData *interfaceData = ie->getProtocolData<Ipv4InterfaceData>();

            interfaceData->setIPAddress(address);
            interfaceData->setNetmask(netmask);
            ie->setBroadcast(true);

            interfaceData->joinMulticastGroup(Ipv4Address::ALL_HOSTS_MCAST);
            interfaceData->joinMulticastGroup(Ipv4Address::ALL_ROUTERS_MCAST);

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

            configurator->addNextHopInterface(mod, destMod, ie->getInterfaceId());
            //add
            //configurator->assignAddress(InterfaceEntry1)
            //node->configureInterface(InterfaceEntry1)
            // configure routing table?
        }
    }
}

void LeoChannelConstructor::prepareInterface(InterfaceEntry *interfaceEntry)
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

void LeoChannelConstructor::createChannel(std::string delay, cGate *gate1, cGate*gate2)
{
    cChannelType *channelType = cChannelType::get("ned.DatarateChannel");
    cChannel *channel = channelType->create("channel");
    channel->par("delay").parse(delay.c_str());
    //channel->par("datarate").parse(linkDataRate.c_str());
    channel->par("datarate").parse("10Mbps");
    gate1->connectTo(gate2, channel);
}

bool LeoChannelConstructor::handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback)
{
    return true;
}
void LeoChannelConstructor::finish()
{

}

}
