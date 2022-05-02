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

#include "LeoNetworkConfigurator.h"

#include "MatcherOS3.h"

#include <set>

#include "MatcherOS3.h"
#include "inet/common/INETUtils.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/stlutils.h"
#include "inet/common/XMLUtils.h"
#include "inet/networklayer/common/InterfaceEntry.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/networklayer/configurator/ipv4/Ipv4NetworkConfigurator.h"
#include "inet/networklayer/ipv4/IIpv4RoutingTable.h"
#include "mobility/SatSGP4Mobility.h"
#include "mobility/LUTMotionMobility.h"
#include "../../../mobility/GroundStationMobility.h"
#include "../../../mobility/SatelliteMobility.h"
#include "../../../mobility/NoradA.h"
#include "../../../libnorad/cEcef.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"
#include "inet/common/ProtocolTag_m.h"
#ifdef WITH_RADIO
#include "inet/physicallayer/base/packetlevel/FlatReceiverBase.h"
#include "inet/physicallayer/base/packetlevel/FlatTransmitterBase.h"
#include "inet/physicallayer/common/packetlevel/Interference.h"
#include "inet/physicallayer/common/packetlevel/Radio.h"
#include "inet/physicallayer/common/packetlevel/ReceptionDecision.h"
#include "inet/physicallayer/contract/packetlevel/IRadio.h"
#include "inet/physicallayer/contract/packetlevel/IRadioMedium.h"
#include "inet/physicallayer/contract/packetlevel/SignalTag_m.h"
#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
#include "../../ipv4/LeoIpv4RoutingTable.h"
#endif
Define_Module(LeoNetworkConfigurator);
namespace inet {
#ifdef WITH_RADIO
using namespace inet::physicallayer;
#endif

/**
 * The LeoNetworkConfigurator model inherits from the Ipv4NetworkConfigurator model so that the
 * Dijkstra's shortest path algorithm than is used within the Ipv4NetworkConfigurator can be repeatedly
 * called throughout the simulation rather than only at the beginning. A large amount of changes was required
 * however as the Ipv4NetworkConfigurator was not made for constant reinvoking. In summary, the model works by
 * the following steps:
 *
 * 1) An OMNeT++ cMessage timer is set which upon being handled calls a method which re invokes the following
 * steps every x milliseconds simulation time.
 *
 * 2) A graph is built which represents the current satellite network topology. Every node (satellites and ground
 * stations) within the network has a @networkNode  property which indicates it should be a vertex within the graph.
 * Edges are created based on whether or not a node is within the communication range of the radio medium UnitDiskRadio.
 *
 * 3) Using methods entirely obtained from the Ipv4NetworkConfigurator, IP addresses are assigned to every interface for all
 * network nodes. The configurator uses an address template of "10.0.x.x" for every node within the network, where each subnet
 *  has a maximum of 255 hosts. For example, satellite number 255 will have the address "10.0.0.255", while satellite number
 *  256 will have the address "10.0.1.0". Assigned addresses always maximise the possible nodes of a subset.
 *
 * 4) This step is where the primary link filtering takes place. If any links are not reflective of Starlink's specification,
 * such as ground stations connecting to over ground-stations, the links are disabled within the graph topology. For any links
 * that have not been disabled in the graph, edge weights are set by sending mock transmissions to determine whether a connection
 * can be made. If the connection cannot be made (outside of the radio medium range) an infinite weight is given to the edge.
 * Otherwise the propagation delay between the two vertices is set as the edge weight. Internally all links are also stored within
 * a table.
 *
 * 5) Using Dijkstra's algorithm, all of the "static" routes of the network are added to each nodes routing tables.
 *
 * 6) Once the timer message is handled, the reinvokeConfiguration method called. Everything that needs to be rebuilt is cleared, including
 * the past graph topology and node routing tables. Step 1, 2, 4, 5 and 6 are then repeated for the entire length of the simulation. The
 * IP addresses are not reconfigured as they have already been set for each node.
 */

void LeoNetworkConfigurator::initialize(int stage)
{
    timerInterval = par("updateInterval"); //Obtain the update interval from the INI file.
    if(timer != nullptr){                 //Prevent memory leak
        cancelAndDelete(timer);
    }
    timer = new cMessage("TopologyTimer");
    scheduleAt(0 + timerInterval, timer);  //Schedule reinvoking process.
    NetworkConfiguratorBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        assignAddressesParameter = par("assignAddresses");
        assignUniqueAddresses = par("assignUniqueAddresses");
        assignDisjunctSubnetAddressesParameter = par("assignDisjunctSubnetAddresses");
        addStaticRoutesParameter = par("addStaticRoutes");
        addSubnetRoutesParameter = par("addSubnetRoutes");
        addDefaultRoutesParameter = par("addDefaultRoutes");
        addDirectRoutesParameter = par("addDirectRoutes");
        optimizeRoutesParameter = par("optimizeRoutes");
        enableInterSatelliteLinksParameter = par("enableInterSatelliteLinks");
    }
    else if (stage == INITSTAGE_NETWORK_CONFIGURATION){
        linksEstablished = true;
        ensureConfigurationComputed(topology);
    }
    else if (stage == INITSTAGE_LAST)
        dumpConfiguration();
}

void LeoNetworkConfigurator::reinvokeConfigure(){
    //assignAddresses(topology);
    cXMLElementList autorouteElements = configuration->getChildrenByTagName("autoroute");
    if (autorouteElements.size() == 0) {
        cXMLElement defaultAutorouteElement("autoroute", "", nullptr);
        reinvokeConfigurator(topology, &defaultAutorouteElement);
    }
    else {
        for (auto & autorouteElement : autorouteElements)
            reinvokeConfigurator(topology, autorouteElement);
    }
}

void LeoNetworkConfigurator::configure(){
    //topology.clear();
    linksEstablished = true;
    LeoNetworkConfigurator::ensureConfigurationComputed(topology);
    configureAllInterfaces();
    configureAllRoutingTables();
}

void LeoNetworkConfigurator::configureInterface(InterfaceEntry *interfaceEntry)
{
    if(linksEstablished){
        ensureConfigurationComputed(topology);
        auto it = topology.interfaceInfos.find(interfaceEntry->getId());
        if (it != topology.interfaceInfos.end()) {
            InterfaceInfo *interfaceInfo = static_cast<InterfaceInfo *>(it->second);
            if (interfaceInfo->configure)
                Ipv4NetworkConfigurator::configureInterface(interfaceInfo);
        }
    }
}

/**
 * This method handles the timer that calls the reinvoking process. Once the routes have again been determined
 * the timer is rescheduled.
 */
void LeoNetworkConfigurator::handleMessage(cMessage *msg)
{
    if (msg == timer) {
        cXMLElementList autorouteElements = configuration->getChildrenByTagName("autoroute");
                if (autorouteElements.size() == 0) {
                    cXMLElement defaultAutorouteElement("autoroute", "", nullptr);
                    LeoNetworkConfigurator::reinvokeConfigurator(topology, &defaultAutorouteElement);
                }
                else {
                    for (auto & autorouteElement : autorouteElements)
                        LeoNetworkConfigurator::reinvokeConfigurator(topology, autorouteElement);
                }
       scheduleAt(simTime() + timerInterval, timer);  // rescheduling
    }
}

/**
 * The reinvokeConfigurator method is responsible for calling the required methods to re obtain all of the static
 * routes and update the routing table of each node. The intial step is to extract the topology, where a Topology object
 * is created where each node corresponds to a @networkNode. All links are also obtained from this extraction process.
 * All of the node and topology information is cleared before the topology is extracted again to ensure that no past information
 * about the previous topology is remembered. The addresses of every interface have already been established before the start of
 * the simulation, therefore they are not reassigned as this would be a pointless implementation. The addStaticRoutes method is then
 * called which firstly filters all of the unwanted links such as ground-station to ground-station links, links that do not satisfy
 * the minumum elevation angle, and links that are not suitable as inter-satellite links if they are enabled. Dijkstra's is then
 * run within this method and the routing tables for all nodes are configured.
 */
void LeoNetworkConfigurator::reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement)
{
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        //std::for_each(node->interfaceInfos.begin(), node->interfaceInfos.end(), [](InterfaceInfo* interface) { delete interface; });
        //for( auto & p : node->interfaceInfos ) delete p;
        node->interfaceInfos.clear();
        LeoIpv4RoutingTable *routingTable = dynamic_cast<LeoIpv4RoutingTable*>(node->routingTable);
        for(int j = 0; j < routingTable->getNumRoutes(); j++){
            //delete node->routingTable->routes;
            //Ipv4RoutingTable *routingTable = dynamic_cast<Ipv4RoutingTable*>(node->routingTable); //TO DO, change it so it isnt restricted to ipv4
            bool check = routingTable->deleteRoute(routingTable->getRoute(j));
            EV_DEBUG << check;
        }
        for(int m = 0; m < node->routingTable->getNumMulticastRoutes(); m++){
            node->routingTable->deleteMulticastRoute(node->routingTable->getMulticastRoute(m));
        }
        //routingTable->Ipv4RoutingTable;
        std::for_each(node->staticRoutes.begin(), node->staticRoutes.end(), []( Ipv4Route* route) { delete route; });
                //for every route, pop each route off.
        node->staticRoutes.clear();
        routingTable->configureRouterId();
    }
    std::for_each(topology.linkInfos.begin(), topology.linkInfos.end(), []( LinkInfo* link) { delete link; });
    //std::for_each(topology.interfaceInfos.begin(), topology.interfaceInfos.end(), [](map::iterator iter) { delete (*iter).second; });
    for( auto & p : topology.interfaceInfos ) delete p.second;
    topology.linkInfos.clear();
    topology.interfaceInfos.clear();
    topology.clear();
    LeoNetworkConfigurator::extractTopology(topology);
    //assignNewAddresses();
    //readInterfaceConfiguration(topology);
    //assignAddresses(topology);
    //readMulticastGroupConfiguration(topology);
    //readManualRouteConfiguration(topology);
    //readManualMulticastRouteConfiguration(topology);
    //readInterfaceConfiguration(topology);
    //assignAddresses(topology);
    addStaticRoutes(topology, autorouteElement);
    configureAllInterfaces();
    LeoNetworkConfigurator::configureAllRoutingTables();
}

void LeoNetworkConfigurator::assignNewAddress(cModule *mod)
{
    //topology.linkInfos
    // for loop through InterfaceInfos
    //get each InterfaceEntry
    // check if configured, if it is - ignore it
    // otherwise generate new IP address.
    //maybe make permanent list of already defined IP addresses for ease of access.
//    int bitSize = sizeof(uint32) * 8;
//
//    // iterate through all links and process them separately one by one
//    std::vector<LinkInfo *> links = topology.linkInfos;
//    for (auto & selectedLink : links) {
//
//        std::vector<InterfaceInfo *> unconfiguredInterfaces;
//        for(auto & interface : selectedLink->interfaceInfos){
//            InterfaceInfo *candidateInterface = static_cast<InterfaceInfo *>(interface);
//            if(candidateInterface->address == 0)
//                unconfiguredInterfaces.push_back(candidateInterface);
//        }
//        for (auto & compatibleInterface : unconfiguredInterfaces) {
//            InterfaceEntry *interfaceEntry = compatibleInterface->interfaceEntry;
//            uint32 interfaceAddress = compatibleInterface->address;
//            uint32 interfaceAddressSpecifiedBits = compatibleInterface->addressSpecifiedBits;
//            uint32 interfaceAddressUnspecifiedBits = ~interfaceAddressSpecifiedBits;    // 1 means the interface address is unspecified
//            std::cout << "\nAddress: " << interfaceAddress << endl;
//            std::cout << "\nAddress Specified Bits: " << interfaceAddressSpecifiedBits << endl;
//            std::cout << "\nNetmask: " << compatibleInterface->netmask << endl;
//            std::cout << "\nNetmask Specified Bits: " << compatibleInterface->netmaskSpecifiedBits << endl;
//        }
//    }

    std::string interfaces = "ppp[0]";
    Ipv4Address addressBase = Ipv4Address("10.0.0.0");    // start of address range from which to automatically assign an address to the autoassignInterfaces
    Ipv4Address netmask = Ipv4Address("255.0.0.0");
    IInterfaceTable *interfaceTable;

    Ipv4Address myAddress = Ipv4Address(addressBase.getInt() + uint32(mod->getId()));

    cStringTokenizer interfaceTokenizer(interfaces.c_str());
    const char *ifname;
    uint32_t loopbackAddr = Ipv4Address::LOOPBACK_ADDRESS.getInt();
    while ((ifname = interfaceTokenizer.nextToken()) != nullptr) {
        InterfaceEntry *ie = interfaceTable->findInterfaceByName(ifname);
        if (!ie)
            throw cRuntimeError("No such interface '%s'", ifname);

        auto ipv4Data = ie->getProtocolData<Ipv4InterfaceData>();
        // assign IP Address to all connected interfaces
        if (ie->isLoopback()) {
            ipv4Data->setIPAddress(Ipv4Address(loopbackAddr++));
            ipv4Data->setNetmask(Ipv4Address::LOOPBACK_NETMASK);
            ipv4Data->setMetric(1);
            EV_INFO << "loopback interface " << ifname << " gets " << ipv4Data->getIPAddress() << "/" << ipv4Data->getNetmask() << std::endl;
            continue;
        }

        EV_INFO << "interface " << ifname << " gets " << myAddress.str() << "/" << netmask.str() << std::endl;

        ipv4Data->setIPAddress(myAddress);
        ipv4Data->setNetmask(netmask);
        ie->setBroadcast(true);

        // associate interface with default multicast groups
        ipv4Data->joinMulticastGroup(Ipv4Address::ALL_HOSTS_MCAST);
        ipv4Data->joinMulticastGroup(Ipv4Address::ALL_ROUTERS_MCAST);

//        // associate interface with specified multicast groups
//        cStringTokenizer interfaceTokenizer(mcastGroups.c_str());
//        const char *mcastGroup_s;
//        while ((mcastGroup_s = interfaceTokenizer.nextToken()) != nullptr) {
//            Ipv4Address mcastGroup(mcastGroup_s);
//            ipv4Data->joinMulticastGroup(mcastGroup);
//        }
    }
}

/**
 * Same implementation from the Ipv4NetworkConfigurator but it is needed to make sure that
 * the LeoNetworkConfigurator configureRoutingTable(Node *node) method is called as
 * this includes a unique implementation.
 */
void LeoNetworkConfigurator::configureAllRoutingTables()
{
    ensureConfigurationComputed(topology);
    EV_INFO << "Configuring all routing tables.\n";
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        if (node->routingTable)
            LeoNetworkConfigurator::configureRoutingTable(node);
    }
}

/**
 * Same implementation from the Ipv4NetworkConfigurator but it is needed to make sure that
 * the LeoNetworkConfigurator configureRoutingTable(Node *node) method is called as
 * this includes a unique implementation.
 */
void LeoNetworkConfigurator::configureRoutingTable(IIpv4RoutingTable *routingTable)
{
    if(linksEstablished){
        ensureConfigurationComputed(topology);
        // TODO: avoid linear search
        for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            if (node->routingTable == routingTable)
                configureRoutingTable(node);
        }
    }
}


/**
 * Overriden method from the IPv4 Configurator. As the routing tables will need to be configured multiple times,
 * routes may appear twice unless they are removed. This method will make sure that duplicated routes within the tables
 * are disregarded/replaced accordingly.
 */
void LeoNetworkConfigurator::configureRoutingTable(Node *node)
{
    EV_DETAIL << "Configuring routing table of " << node->getModule()->getFullPath() << ".\n";
    for (size_t i = 0; i < node->staticRoutes.size(); i++) {
        Ipv4Route *original = node->staticRoutes[i];
        Ipv4Route *clone = new Ipv4Route();
        if(i < node->staticRoutes.size()){
            bool dupe = false;
            for (int j = 0; j < node->routingTable->getNumRoutes(); j++){
                if(original->getDestinationAsGeneric() == node->routingTable->getRoute(j)->getDestinationAsGeneric()){
                    node->routingTable->getRoute(j)->setNextHop(original->getNextHopAsGeneric()); //If the destination is already in the routing table, the destination will be updated with the new found better hop.
                    dupe = true;
                    delete clone;
                }
            }
            if(dupe != true){ //If no duplication occurs, the link can be added as usual. Otherwise it is ignored.
                clone->setMetric(original->getMetric());
                clone->setSourceType(original->getSourceType());
                clone->setSource(original->getSource());
                clone->setDestination(original->getDestination());
                clone->setNetmask(original->getNetmask());
                clone->setGateway(original->getGateway());
                clone->setInterface(original->getInterface());
                node->routingTable->addRoute(clone);
            }

        }
    }
    /**
     * Multicast route implementation that is not used for the simulation model. This was implemented in case
     * future experiments require it.
     */
    for (size_t i = 0; i < node->staticMulticastRoutes.size(); i++) {
        Ipv4MulticastRoute *original = node->staticMulticastRoutes[i];
        Ipv4MulticastRoute *clone = new Ipv4MulticastRoute();
        clone->setMetric(original->getMetric());
        clone->setSourceType(original->getSourceType());
        clone->setSource(original->getSource());
        clone->setOrigin(original->getOrigin());
        clone->setOriginNetmask(original->getOriginNetmask());
        clone->setInInterface(original->getInInterface());
        clone->setMulticastGroup(original->getMulticastGroup());
        for (size_t j = 0; j < original->getNumOutInterfaces(); j++)
            clone->addOutInterface(new IMulticastRoute::OutInterface(*original->getOutInterface(j)));
        node->routingTable->addMulticastRoute(clone);
    }
}

}
