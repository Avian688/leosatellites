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

#include "SatelliteNetworkConfigurator.h"

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
#include "inet/networklayer/configurator/base/NetworkConfiguratorBase.h"
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
#endif
Define_Module(SatelliteNetworkConfigurator);
namespace inet {
#ifdef WITH_RADIO
using namespace inet::physicallayer;
#endif

/**
 * The SatelliteNetworkConfigurator model inherits from the Ipv4NetworkConfigurator model so that the
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

void SatelliteNetworkConfigurator::initialize(int stage)
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
    else if (stage == INITSTAGE_NETWORK_CONFIGURATION)
        ensureConfigurationComputed(topology);
    else if (stage == INITSTAGE_LAST)
        dumpConfiguration();
}

/**
 * This method handles the timer that calls the reinvoking process. Once the routes have again been determined
 * the timer is rescheduled.
 */
void SatelliteNetworkConfigurator::handleMessage(cMessage *msg)
{
    if (msg == timer) {
        cXMLElementList autorouteElements = configuration->getChildrenByTagName("autoroute");
                if (autorouteElements.size() == 0) {
                    cXMLElement defaultAutorouteElement("autoroute", "", nullptr);
                    SatelliteNetworkConfigurator::reinvokeConfigurator(topology, &defaultAutorouteElement);
                }
                else {
                    for (auto & autorouteElement : autorouteElements)
                        SatelliteNetworkConfigurator::reinvokeConfigurator(topology, autorouteElement);
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
void SatelliteNetworkConfigurator::reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement)
{
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        //std::for_each(node->interfaceInfos.begin(), node->interfaceInfos.end(), [](InterfaceInfo* interface) { delete interface; });
        //for( auto & p : node->interfaceInfos ) delete p;
        node->interfaceInfos.clear();
        Ipv4RoutingTable *routingTable = dynamic_cast<Ipv4RoutingTable*>(node->routingTable);
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
     }
    std::for_each(topology.linkInfos.begin(), topology.linkInfos.end(), []( LinkInfo* link) { delete link; });
    //std::for_each(topology.interfaceInfos.begin(), topology.interfaceInfos.end(), [](map::iterator iter) { delete (*iter).second; });
    for( auto & p : topology.interfaceInfos ) delete p.second;
    topology.linkInfos.clear();
    topology.interfaceInfos.clear();
    topology.clear();

    SatelliteNetworkConfigurator::extractTopology(topology);

    SatelliteNetworkConfigurator::addStaticRoutes(topology, autorouteElement);

    SatelliteNetworkConfigurator::configureAllRoutingTables();

}

/**
 * Same implementation from the Ipv4NetworkConfigurator but it is needed to make sure that
 * the SatelliteNetworkConfigurator configureRoutingTable(Node *node) method is called as
 * this includes a unique implementation.
 */
void SatelliteNetworkConfigurator::configureAllRoutingTables()
{
    ensureConfigurationComputed(topology);
    EV_INFO << "Configuring all routing tables.\n";
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        if (node->routingTable)
            SatelliteNetworkConfigurator::configureRoutingTable(node);
    }
}

/**
 * Same implementation from the Ipv4NetworkConfigurator but it is needed to make sure that
 * the SatelliteNetworkConfigurator configureRoutingTable(Node *node) method is called as
 * this includes a unique implementation.
 */
void SatelliteNetworkConfigurator::configureRoutingTable(IIpv4RoutingTable *routingTable)
{
    ensureConfigurationComputed(topology);
    // TODO: avoid linear search
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        if (node->routingTable == routingTable)
            configureRoutingTable(node);
    }
}


/**
 * Overriden method from the IPv4 Configurator. As the routing tables will need to be configured multiple times,
 * routes may appear twice unless they are removed. This method will make sure that duplicated routes within the tables
 * are disregarded/replaced accordingly.
 */
void SatelliteNetworkConfigurator::configureRoutingTable(Node *node)
{
    EV_DETAIL << "Configuring routing table of " << node->getModule()->getFullPath() << ".\n";
    for (size_t i = 0; i < node->staticRoutes.size(); i++) {
        Ipv4Route *original = node->staticRoutes[i];
        Ipv4Route *clone = new Ipv4Route();
        if(i < node->staticRoutes.size()){
            bool dupe = false;
            for (size_t j = 0; j < node->routingTable->getNumRoutes(); j++){
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

/**
 * This method is an override of the respective IPv4 Configurator method. The method has been altered
 * so that the LEO constellation links are filtered according to FCC information. For example a satellite
 * cannot connect to a ground station that is more than 1123km in distance. This method also controls the
 * inter-satellite links by disabling any link that is not within the correct orbital plane. Currently
 * inter-satellite links only work with the NoradA class, not Norad (TLE data).
 */
void SatelliteNetworkConfigurator::addStaticRoutes(Topology& topology, cXMLElement *autorouteElement)
{
    // set node weights
    const char *metric = autorouteElement->getAttribute("metric");
    if (metric == nullptr)
        metric = "hopCount";
    cXMLElement defaultNodeElement("node", "", nullptr);
    cXMLElementList nodeElements = autorouteElement->getChildrenByTagName("node");
    for (int i = 0; i < topology.getNumNodes(); i++) {
        cXMLElement *selectedNodeElement = &defaultNodeElement;
        Node *node = (Node *)topology.getNode(i);
        for (auto & nodeElement : nodeElements) {
            const char* hosts = nodeElement->getAttribute("hosts");
            if (hosts == nullptr)
                hosts = "**";
            MatcherOS3 nodeHostsMatcher(hosts);
            std::string hostFullPath = node->module->getFullPath();
            std::string hostShortenedFullPath = hostFullPath.substr(hostFullPath.find('.') + 1);
            if (nodeHostsMatcher.matchesAny() || nodeHostsMatcher.matches(hostShortenedFullPath.c_str()) || nodeHostsMatcher.matches(hostFullPath.c_str())) {
                selectedNodeElement = nodeElement;
                break;
            }
        }
        double weight = computeNodeWeight(node, metric, selectedNodeElement);
        EV_DEBUG << "Setting node weight, node = " << node->module->getFullPath() << ", weight = " << weight << endl;
        node->setWeight(weight);
    }
    // set link weights
    cXMLElement defaultLinkElement("link", "", nullptr);
    cXMLElementList linkElements = autorouteElement->getChildrenByTagName("link");
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        for (int j = 0; j < node->getNumInLinks(); j++) {
            bool linkDisabled = false;
            cXMLElement *selectedLinkElement = &defaultLinkElement;
            Link *link = (Link *)node->getLinkIn(j);

            //Filter links within the topology:

            cModule *sourceModule = link->sourceInterfaceInfo->node->module;
            cModule *destModule = link->destinationInterfaceInfo->node->module;

            if(LUTMotionMobility *sourceLutMobility = dynamic_cast<LUTMotionMobility*>(sourceModule->getSubmodule("mobility")))
            {
                if(dynamic_cast<LUTMotionMobility*>(destModule->getSubmodule("mobility")))
                {
                    //Disable ground station to ground station links
                    link->disable();
                    linkDisabled=true; //make sure that the link weight is not assigned as it is not required if disabled

                }
                //Ground to satellite - make sure links are correct distance
                else if(SatelliteMobility *destSatMobility = dynamic_cast<SatelliteMobility*>(destModule->getSubmodule("mobility")))
                {
                    if(destSatMobility->getDistance(sourceLutMobility->getLUTPositionY(), sourceLutMobility->getLUTPositionX(), 0) >= 1123) //out of range angle
                    {
                        link->disable();
                        linkDisabled=true;
                    }
                }
            }

            if(SatelliteMobility *sourceSatMobility = dynamic_cast<SatelliteMobility*>(sourceModule->getSubmodule("mobility")))
            {
                if(dynamic_cast<SatelliteMobility*>(destModule->getSubmodule("mobility")))
                {
                    //Disable inter-satellite links:
                    if(enableInterSatelliteLinksParameter){
                        NoradA *sourceNoradModule = dynamic_cast<NoradA*>(sourceModule->getSubmodule("NoradModule", 0));
                        NoradA *destNoradModule = dynamic_cast<NoradA*>(destModule->getSubmodule("NoradModule", 0));
                        const int satNum2 = destNoradModule->getSatelliteNumber();
                        bool validLink = false;
                        //if the satellites are part of the same phase - check inter-satellite links
                        if((sourceNoradModule->getSatellitesPerPlane() == destNoradModule->getSatellitesPerPlane()) && //potentially move check into NoradA class
                            (sourceNoradModule->getNumberOfPlanes() == destNoradModule->getNumberOfPlanes())&&
                            (sourceNoradModule->getInclination() == destNoradModule->getInclination()))
                        {
                            if(sourceNoradModule->isInterSatelliteLink(satNum2))
                            {
                                validLink = true; //Valid Inter-Satellite link as described by the NoradA module.
                            }
                        }

                        if(validLink != true)
                        {
                            link->disable();
                            linkDisabled=true;
                        }
                    }
                    else{
                        link->disable(); //if the enableInterSatelliteLinksParameter is disabled. Only relays are considered therefore all inter-satellite links are disabled.
                        linkDisabled=true;
                    }
                }
                else if(LUTMotionMobility *destLutMobility = dynamic_cast<LUTMotionMobility*>(destModule->getSubmodule("mobility")))
                {
                    if(sourceSatMobility->getDistance(destLutMobility->getLUTPositionY(), destLutMobility->getLUTPositionX(), 0) >= 1123) //out of range angle
                    {
                        link->disable(); //Ground to Satellite, make sure the maximum distance is satisfied, else disable it.
                        linkDisabled=true;
                    }
                }
            }

            //Boolean parameter added. The following block is not required if the link is disabled.
            if(!linkDisabled){
                for (auto & linkElement : linkElements) {
                    const char* interfaces = linkElement->getAttribute("interfaces");
                    if (interfaces == nullptr)
                        interfaces = "**";
                    MatcherOS3 linkInterfaceMatcher(interfaces);
                    std::string sourceFullPath = link->sourceInterfaceInfo->getFullPath();
                    std::string sourceShortenedFullPath = sourceFullPath.substr(sourceFullPath.find('.') + 1);
                    std::string destinationFullPath = link->destinationInterfaceInfo->getFullPath();
                    std::string destinationShortenedFullPath = destinationFullPath.substr(destinationFullPath.find('.') + 1);
                    if (linkInterfaceMatcher.matchesAny() ||
                        linkInterfaceMatcher.matches(sourceFullPath.c_str()) || linkInterfaceMatcher.matches(sourceShortenedFullPath.c_str()) ||
                        linkInterfaceMatcher.matches(destinationFullPath.c_str()) || linkInterfaceMatcher.matches(destinationShortenedFullPath.c_str()))
                    {
                        selectedLinkElement = linkElement;
                        break;
                    }
                }
                double weight = computeLinkWeight(link, metric, selectedLinkElement);
                EV_DEBUG << "Setting link weight, link = " << link << ", weight = " << weight << endl;
                link->setWeight(weight);
            }
        }
    }
    // add static routes for all routing tables
    const char* sourceHosts = autorouteElement->getAttribute("sourceHosts");
    if (sourceHosts == nullptr)
        sourceHosts = "**";
    const char* destinationInterfaces = autorouteElement->getAttribute("destinationInterfaces");
    if (destinationInterfaces == nullptr)
        destinationInterfaces = "**";
    MatcherOS3 sourceHostsMatcher(sourceHosts);
    MatcherOS3 destinationInterfacesMatcher(destinationInterfaces);
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *sourceNode = (Node *)topology.getNode(i);
        std::string hostFullPath = sourceNode->module->getFullPath();
        std::string hostShortenedFullPath = hostFullPath.substr(hostFullPath.find('.') + 1);
        if (!sourceHostsMatcher.matchesAny() && !sourceHostsMatcher.matches(hostShortenedFullPath.c_str()) && !sourceHostsMatcher.matches(hostFullPath.c_str()))
            continue;
        if (isBridgeNode(sourceNode))
            continue;
        // calculate shortest paths from everywhere to sourceNode
        // we are going to use the paths in reverse direction (assuming all links are bidirectional)
        topology.calculateWeightedSingleShortestPathsTo(sourceNode);
        // check if adding the default routes would be ok (this is an optimization)
        if (addDefaultRoutesParameter && sourceNode->interfaceInfos.size() == 1 && sourceNode->interfaceInfos[0]->linkInfo->gatewayInterfaceInfo && sourceNode->interfaceInfos[0]->addDefaultRoute) {
            InterfaceInfo *sourceInterfaceInfo = static_cast<InterfaceInfo *>(sourceNode->interfaceInfos[0]);
            InterfaceEntry *sourceInterfaceEntry = sourceInterfaceInfo->interfaceEntry;
            InterfaceInfo *gatewayInterfaceInfo = static_cast<InterfaceInfo *>(sourceInterfaceInfo->linkInfo->gatewayInterfaceInfo);
            //InterfaceEntry *gatewayInterfaceEntry = gatewayInterfaceInfo->interfaceEntry;

            if (addDirectRoutesParameter) {
                // add a network route for the local network using ARP
                Ipv4Route *route = new Ipv4Route();
                route->setDestination(sourceInterfaceInfo->getAddress().doAnd(sourceInterfaceInfo->getNetmask()));
                route->setGateway(Ipv4Address::UNSPECIFIED_ADDRESS);
                route->setNetmask(sourceInterfaceInfo->getNetmask());
                route->setInterface(sourceInterfaceEntry);
                route->setSourceType(Ipv4Route::MANUAL);
                sourceNode->staticRoutes.push_back(route);
            }

            // add a default route towards the only one gateway
            Ipv4Route *route = new Ipv4Route();
            Ipv4Address gateway = gatewayInterfaceInfo->getAddress();
            route->setDestination(Ipv4Address::UNSPECIFIED_ADDRESS);
            route->setNetmask(Ipv4Address::UNSPECIFIED_ADDRESS);
            route->setGateway(gateway);
            route->setInterface(sourceInterfaceEntry);
            route->setSourceType(Ipv4Route::MANUAL);
            sourceNode->staticRoutes.push_back(route);

            // skip building and optimizing the whole routing table
            EV_DEBUG << "Adding default routes to " << sourceNode->getModule()->getFullPath() << ", node has only one (non-loopback) interface\n";
        }
        else {
            // add a route to all destinations in the network
            for (int j = 0; j < topology.getNumNodes(); j++) {
                // extract destination
                Node *destinationNode = (Node *)topology.getNode(j);
                if (sourceNode == destinationNode)
                    continue;
                if (destinationNode->getNumPaths() == 0)
                    continue;
                if (isBridgeNode(destinationNode))
                    continue;

                // determine next hop interface
                // find next hop interface (the last IP interface on the path that is not in the source node)
                Node *node = destinationNode;
                Link *link = nullptr;
                InterfaceInfo *nextHopInterfaceInfo = nullptr;
                while (node != sourceNode) {
                    link = (Link *)node->getPath(0);
                    if (node != sourceNode && !isBridgeNode(node) && link->sourceInterfaceInfo)
                        nextHopInterfaceInfo = static_cast<InterfaceInfo *>(link->sourceInterfaceInfo);
                    node = (Node *)node->getPath(0)->getRemoteNode();
                }

                // determine source interface
                if (nextHopInterfaceInfo && link->destinationInterfaceInfo && link->destinationInterfaceInfo->addStaticRoute) {
                    InterfaceEntry *sourceInterfaceEntry = link->destinationInterfaceInfo->interfaceEntry;
                    // add the same routes for all destination interfaces (IP packets are accepted from any interface at the destination)
                    for (size_t j = 0; j < destinationNode->interfaceInfos.size(); j++) {
                        InterfaceInfo *destinationInterfaceInfo = static_cast<InterfaceInfo *>(destinationNode->interfaceInfos[j]);
                        std::string destinationFullPath = destinationInterfaceInfo->interfaceEntry->getInterfaceFullPath();
                        std::string destinationShortenedFullPath = destinationFullPath.substr(destinationFullPath.find('.') + 1);
                        if (!destinationInterfacesMatcher.matchesAny() &&
                            !destinationInterfacesMatcher.matches(destinationFullPath.c_str()) &&
                            !destinationInterfacesMatcher.matches(destinationShortenedFullPath.c_str()))
                            continue;
                        InterfaceEntry *destinationInterfaceEntry = destinationInterfaceInfo->interfaceEntry;
                        Ipv4Address destinationAddress = destinationInterfaceInfo->getAddress();
                        Ipv4Address destinationNetmask = destinationInterfaceInfo->getNetmask();
                        if (!destinationInterfaceEntry->isLoopback() && !destinationAddress.isUnspecified()) {
                            Ipv4Route *route = new Ipv4Route();
                            Ipv4Address gatewayAddress = nextHopInterfaceInfo->getAddress();
                            if (addSubnetRoutesParameter && destinationNode->interfaceInfos.size() == 1 && destinationNode->interfaceInfos[0]->linkInfo->gatewayInterfaceInfo
                                && destinationNode->interfaceInfos[0]->addSubnetRoute)
                            {
                                ASSERT(!destinationAddress.doAnd(destinationNetmask).isUnspecified());
                                route->setDestination(destinationAddress.doAnd(destinationNetmask));
                                route->setNetmask(destinationNetmask);
                            }
                            else {
                                route->setDestination(destinationAddress);
                                route->setNetmask(Ipv4Address::ALLONES_ADDRESS);
                            }
                            route->setInterface(sourceInterfaceEntry);
                            if (gatewayAddress != destinationAddress)
                                route->setGateway(gatewayAddress);
                            route->setSourceType(Ipv4Route::MANUAL);
                            if (containsRoute(sourceNode->staticRoutes, route))
                                delete route;
                            else if (!addDirectRoutesParameter && route->getGateway().isUnspecified())
                                delete route;
                            else {
                                sourceNode->staticRoutes.push_back(route);
                                EV_DEBUG << "Adding route " << sourceInterfaceEntry->getInterfaceFullPath() << " -> " << destinationInterfaceEntry->getInterfaceFullPath() << " as " << route->str() << endl;
                            }
                        }
                    }
                }
            }

            // optimize routing table to save memory and increase lookup performance
            if (optimizeRoutesParameter)
                optimizeRoutes(sourceNode->staticRoutes);
        }
    }
}

/**
 * extractTopology method. This implementation is not changed from the Ipv4NetworkConfigurator. This method extracts
 * the topology from each @networkNode. All links and interfaces are also extracted.
 */
void SatelliteNetworkConfigurator::extractTopology(Topology& topology)
{
    // extract topology
    topology.extractByProperty("networkNode");
    EV_DEBUG << "Topology found " << topology.getNumNodes() << " nodes\n";

    // print isolated networks information
    std::map<int, std::vector<Node *>> isolatedNetworks;
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        int networkId = node->getNetworkId();
        auto networkNodes = isolatedNetworks.find(networkId);
        if (networkNodes == isolatedNetworks.end()) {
            std::vector<Node *> collection = {node};
            isolatedNetworks[networkId] = collection;
        }
        else
            networkNodes->second.push_back(node);
    }
    if (isolatedNetworks.size() == 1)
        EV_DEBUG << "All network nodes belong to a connected network.\n";
    else
        EV_DEBUG << "There exists " << isolatedNetworks.size() << " isolated networks.\n";

    // extract nodes, fill in interfaceTable and routingTable members in node
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        cModule *module = node->getModule();
        node->module = module;
        node->interfaceTable = findInterfaceTable(node);
        node->routingTable = findRoutingTable(node);
    }

    // extract links and interfaces
    std::map<int, InterfaceEntry *> interfacesSeen;
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        IInterfaceTable *interfaceTable = node->interfaceTable;
        if (interfaceTable) {
            for (int j = 0; j < interfaceTable->getNumInterfaces(); j++) {
                InterfaceEntry *interfaceEntry = interfaceTable->getInterface(j);
                if (!interfaceEntry->isLoopback() && interfacesSeen.find(interfaceEntry->getId()) == interfacesSeen.end()) {
                    if (isBridgeNode(node))
                        createInterfaceInfo(topology, node, nullptr, interfaceEntry);
                    else {
                        bool linkPass = false;
                        interfacesSeen[interfaceEntry->getId()] = interfaceEntry;
                        // create a new network link
                        LinkInfo *linkInfo = new LinkInfo();
                        linkInfo->networkId = node->getNetworkId();
                        topology.linkInfos.push_back(linkInfo);
                        // store interface as belonging to the new network link
                        InterfaceInfo *interfaceInfo = createInterfaceInfo(topology, node, isBridgeNode(node) ? nullptr : linkInfo, interfaceEntry);
                        linkInfo->interfaceInfos.push_back(interfaceInfo);
                        // visit neighbors (and potentially the whole LAN, recursively)
                        if(linkPass){
                            if (isWirelessInterface(interfaceEntry)) {
                                std::vector<Node *> empty;
                                auto wirelessId = getWirelessId(interfaceEntry);
                                extractWirelessNeighbors(topology, wirelessId.c_str(), linkInfo, interfacesSeen, empty);
                            }
                            else {
                                Topology::LinkOut *linkOut = findLinkOut(node, interfaceEntry->getNodeOutputGateId());
                                if (linkOut) {
                                    std::vector<Node *> empty;
                                    extractWiredNeighbors(topology, linkOut, linkInfo, interfacesSeen, empty);
                                }
                            }
                        }

                    }
                }
            }
        }
    }

    // annotate links with interfaces
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        for (int j = 0; j < node->getNumOutLinks(); j++) {
            Topology::LinkOut *linkOut = node->getLinkOut(j);
            Link *link = (Link *)linkOut;
            Node *localNode = (Node *)linkOut->getLocalNode();
            if (localNode->interfaceTable)
                link->sourceInterfaceInfo = findInterfaceInfo(localNode, localNode->interfaceTable->findInterfaceByNodeOutputGateId(linkOut->getLocalGateId()));
            Node *remoteNode = (Node *)linkOut->getRemoteNode();
            if (remoteNode->interfaceTable)
                link->destinationInterfaceInfo = findInterfaceInfo(remoteNode, remoteNode->interfaceTable->findInterfaceByNodeInputGateId(linkOut->getRemoteGateId()));
        }
    }

    // collect wireless LAN interface infos into a map
    std::map<std::string, std::vector<inet::NetworkConfiguratorBase::InterfaceInfo *> > wirelessIdToInterfaceInfosMap;
    for (auto & entry : topology.interfaceInfos) {
        inet::NetworkConfiguratorBase::InterfaceInfo *interfaceInfo = entry.second;
        InterfaceEntry *interfaceEntry = interfaceInfo->interfaceEntry;
        if (!interfaceEntry->isLoopback() && isWirelessInterface(interfaceEntry)) {
            auto wirelessId = getWirelessId(interfaceEntry);
            wirelessIdToInterfaceInfosMap[wirelessId].push_back(interfaceInfo);
        }
    }

    // add extra links between all pairs of wireless interfaces within a LAN (full graph)
    for (auto & entry : wirelessIdToInterfaceInfosMap) {
        std::vector<inet::NetworkConfiguratorBase::InterfaceInfo *>& interfaceInfos = entry.second;
        for (size_t i = 0; i < interfaceInfos.size(); i++) {
            inet::NetworkConfiguratorBase::InterfaceInfo *interfaceInfoI = interfaceInfos.at(i);
            for (size_t j = i + 1; j < interfaceInfos.size(); j++) {
                // assume bidirectional links
                inet::NetworkConfiguratorBase::InterfaceInfo *interfaceInfoJ = interfaceInfos.at(j);
                Link *linkIJ = new Link();
                linkIJ->sourceInterfaceInfo = interfaceInfoI;
                linkIJ->destinationInterfaceInfo = interfaceInfoJ;
                topology.addLink(linkIJ, interfaceInfoI->node, interfaceInfoJ->node);
                Link *linkJI = new Link();
                linkJI->sourceInterfaceInfo = interfaceInfoJ;
                linkJI->destinationInterfaceInfo = interfaceInfoI;
                topology.addLink(linkJI, interfaceInfoJ->node, interfaceInfoI->node);
            }
        }
    }

    // determine gatewayInterfaceInfo for all linkInfos
    for (auto & linkInfo : topology.linkInfos)
        linkInfo->gatewayInterfaceInfo = determineGatewayForLink(linkInfo);
}

/**
 * extractWiredNeighbours method. The semantics have not changed from the Ipv4NetworkConfigurator. This method extracts
 * all wired link connections from a node.
 */
void SatelliteNetworkConfigurator::extractWiredNeighbors(Topology& topology, Topology::LinkOut *linkOut, LinkInfo *linkInfo, std::map<int, InterfaceEntry *>& interfacesSeen, std::vector<Node *>& deviceNodesVisited)
{
    Node *node = (Node *)linkOut->getRemoteNode();
    int inputGateId = linkOut->getRemoteGateId();
    IInterfaceTable *interfaceTable = node->interfaceTable;
    if (!isBridgeNode(node)) {
        InterfaceEntry *interfaceEntry = interfaceTable->findInterfaceByNodeInputGateId(inputGateId);
        if (!interfaceEntry) {
            // no such interface (node is probably down); we should probably get the information from our (future) internal database
        }
        else if (interfacesSeen.find(interfaceEntry->getId()) == interfacesSeen.end()) {
            InterfaceInfo *neighborInterfaceInfo = createInterfaceInfo(topology, node, linkInfo, interfaceEntry);
            linkInfo->interfaceInfos.push_back(neighborInterfaceInfo);
            interfacesSeen[interfaceEntry->getId()] = interfaceEntry;
        }
    }
    else {
        if (!contains(deviceNodesVisited, node))
            extractDeviceNeighbors(topology, node, linkInfo, interfacesSeen, deviceNodesVisited);
    }
}

/**
 * extractWirelessNeighbors method. The semantics has not changed from the Ipv4NetworkConfigurator. This method extracts
 * all wireless link connections from a node.
 */
void SatelliteNetworkConfigurator::extractWirelessNeighbors(Topology& topology, const char *wirelessId, LinkInfo *linkInfo, std::map<int, InterfaceEntry *>& interfacesSeen, std::vector<Node *>& deviceNodesVisited)
{
    for (int nodeIndex = 0; nodeIndex < topology.getNumNodes(); nodeIndex++) {
        Node *node = (Node *)topology.getNode(nodeIndex);
        IInterfaceTable *interfaceTable = node->interfaceTable;
        if (interfaceTable) {
            for (int j = 0; j < interfaceTable->getNumInterfaces(); j++) {
                InterfaceEntry *interfaceEntry = interfaceTable->getInterface(j);
                if (!interfaceEntry->isLoopback() && interfacesSeen.find(interfaceEntry->getId()) == interfacesSeen.end() && isWirelessInterface(interfaceEntry)) {
                    if (getWirelessId(interfaceEntry) == wirelessId) {
                        if (!isBridgeNode(node)) {
                            InterfaceInfo *interfaceInfo = createInterfaceInfo(topology, node, linkInfo, interfaceEntry);
                            linkInfo->interfaceInfos.push_back(interfaceInfo);
                            interfacesSeen[interfaceEntry->getId()] = interfaceEntry;
                        }
                        else {
                            if (!contains(deviceNodesVisited, node))
                                extractDeviceNeighbors(topology, node, linkInfo, interfacesSeen, deviceNodesVisited);
                        }
                    }
                }
            }
        }
    }
}

/**
 * extractDeviceNeighbors method. The semantics have not changed from the Ipv4NetworkConfigurator. This method extracts
 * the neighbors from a given node.
 */
void SatelliteNetworkConfigurator::extractDeviceNeighbors(Topology& topology, Node *node, LinkInfo *linkInfo, std::map<int, InterfaceEntry *>& interfacesSeen, std::vector<Node *>& deviceNodesVisited)
{
    deviceNodesVisited.push_back(node);
    IInterfaceTable *interfaceTable = node->interfaceTable;
    if (interfaceTable) {
        // switch and access point
        for (int i = 0; i < interfaceTable->getNumInterfaces(); i++) {
            InterfaceEntry *interfaceEntry = interfaceTable->getInterface(i);
            if (!interfaceEntry->isLoopback() && interfacesSeen.find(interfaceEntry->getId()) == interfacesSeen.end()) {
                if (isWirelessInterface(interfaceEntry))
                    extractWirelessNeighbors(topology, getWirelessId(interfaceEntry).c_str(), linkInfo, interfacesSeen, deviceNodesVisited);
                else {
                    Topology::LinkOut *linkOut = findLinkOut(node, interfaceEntry->getNodeOutputGateId());
                    if (linkOut)
                        extractWiredNeighbors(topology, linkOut, linkInfo, interfacesSeen, deviceNodesVisited);
                }
            }
        }
    }
    else {
        // hub and bus
        for (int i = 0; i < node->getNumOutLinks(); i++) {
            Topology::LinkOut *linkOut = node->getLinkOut(i);
            extractWiredNeighbors(topology, linkOut, linkInfo, interfacesSeen, deviceNodesVisited);
        }
    }
}

/**
 * This method is needed for use within the computeWirelessWeight method.
 */
static double parseCostAttribute(const char *costAttribute)
{
    if (!strncmp(costAttribute, "inf", 3))
        return INFINITY;
    else {
        double cost = atof(costAttribute);
        if (cost <= 0)
            throw cRuntimeError("Cost cannot be less than or equal to zero");
        return cost;
    }
}

/**
 * This method was adapted from the original computeWirelessLinkWeight method from the Ipv4NetworkConfigurator. The autoroute
 * elements such as hopCount, delay and errorRate were not suitable for the satellite constellations. A new autoroute element,
 * "propagationDelay" was created. This works by setting an infinite weight to any unreachable nodes, while any nearby nodes will
 * have a weight directly proportional to the latency of that link.
 */
double SatelliteNetworkConfigurator::computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
    const char *costAttribute = parameters->getAttribute("cost");
    if (costAttribute != nullptr)
        return parseCostAttribute(costAttribute);
    else {
        if (!strcmp(metric, "hopCount"))
            return 1;
#ifdef WITH_RADIO
        else if (!strcmp(metric, "delay")) {
            // compute the delay between the two interfaces using a dummy transmission
            const NetworkConfiguratorBase::InterfaceInfo *transmitterInterfaceInfo = link->sourceInterfaceInfo;
            const NetworkConfiguratorBase::InterfaceInfo *receiverInterfaceInfo = link->destinationInterfaceInfo;
            cModule *transmitterInterfaceModule = transmitterInterfaceInfo->interfaceEntry;
            cModule *receiverInterfaceModule = receiverInterfaceInfo->interfaceEntry;
            const IRadio *transmitterRadio = check_and_cast<IRadio *>(transmitterInterfaceModule->getSubmodule("radio"));
            const IRadio *receiverRadio = check_and_cast<IRadio *>(receiverInterfaceModule->getSubmodule("radio"));
            const Packet *macFrame = new Packet();
            const IRadioMedium *radioMedium = receiverRadio->getMedium();
            const ITransmission *transmission = transmitterRadio->getTransmitter()->createTransmission(transmitterRadio, macFrame, simTime());
            const IArrival *arrival = radioMedium->getPropagation()->computeArrival(transmission, receiverRadio->getAntenna()->getMobility());
            return arrival->getStartPropagationTime().dbl();
        }
        else if (!strcmp(metric, "propagationDelay")) {
            // compute the packet error rate between the two interfaces using a dummy transmission
            const NetworkConfiguratorBase::InterfaceInfo *transmitterInterfaceInfo = link->sourceInterfaceInfo;
            const NetworkConfiguratorBase::InterfaceInfo *receiverInterfaceInfo = link->destinationInterfaceInfo;
            cModule *transmitterInterfaceModule = transmitterInterfaceInfo->interfaceEntry;
            cModule *receiverInterfaceModule = receiverInterfaceInfo->interfaceEntry;
            const IRadio *transmitterRadio = check_and_cast<IRadio *>(transmitterInterfaceModule->getSubmodule("radio"));
            const IRadio *receiverRadio = check_and_cast<IRadio *>(receiverInterfaceModule->getSubmodule("radio"));
            const IRadioMedium *medium = receiverRadio->getMedium();
            Packet *transmittedFrame = new Packet();
            auto byteCountChunk = makeShared<ByteCountChunk>(B(transmitterInterfaceInfo->interfaceEntry->getMtu()));
            transmittedFrame->insertAtBack(byteCountChunk);

            // KLUDGE:
            transmittedFrame->addTag<PacketProtocolTag>()->setProtocol(nullptr);
            check_and_cast<const Radio *>(transmitterRadio)->encapsulate(transmittedFrame);

            const ITransmission *transmission = transmitterRadio->getTransmitter()->createTransmission(transmitterRadio, transmittedFrame, simTime());
            const IArrival *arrival = medium->getPropagation()->computeArrival(transmission, receiverRadio->getAntenna()->getMobility());
            const IListening *listening = receiverRadio->getReceiver()->createListening(receiverRadio, arrival->getStartTime(), arrival->getEndTime(), arrival->getStartPosition(), arrival->getEndPosition());
            const INoise *noise = medium->getBackgroundNoise() != nullptr ? medium->getBackgroundNoise()->computeNoise(listening) : nullptr;
            const IReception *reception = medium->getAnalogModel()->computeReception(receiverRadio, transmission, arrival);
            const IInterference *interference = new Interference(noise, new std::vector<const IReception *>());
            const ISnir *snir = medium->getAnalogModel()->computeSNIR(reception, noise);
            const IReceiver *receiver = receiverRadio->getReceiver();
            bool isReceptionPossible = receiver->computeIsReceptionPossible(listening, reception, IRadioSignal::SIGNAL_PART_WHOLE);
            double packetErrorRate;
            if (!isReceptionPossible)
                packetErrorRate = 1;
            else {
                const IReceptionDecision *receptionDecision = new ReceptionDecision(reception, IRadioSignal::SIGNAL_PART_WHOLE, isReceptionPossible, true, true);
                const std::vector<const IReceptionDecision *> *receptionDecisions = new std::vector<const IReceptionDecision *> {receptionDecision};
                const IReceptionResult *receptionResult = receiver->computeReceptionResult(listening, reception, interference, snir, receptionDecisions);
                Packet *receivedFrame = const_cast<Packet *>(receptionResult->getPacket());
                packetErrorRate = receivedFrame->getTag<ErrorRateInd>()->getPacketErrorRate();
                delete receptionResult;
                delete receptionDecision;
            }
            delete snir;
            delete interference;
            delete reception;
            delete listening;
            delete arrival;
            delete transmission;
            delete transmittedFrame;

            cModule *transmitterModule = link->sourceInterfaceInfo->node->module;
            cModule *receiverModule= link->destinationInterfaceInfo->node->module;
            double delay = 0;
            //Calculate the propagation delay between the two nodes as part of the link and set the weight.
            if(GroundStationMobility *sourceLutMobility = dynamic_cast<GroundStationMobility*>(transmitterModule->getSubmodule("mobility")))
            {
                if(GroundStationMobility *destLutMobility = dynamic_cast<GroundStationMobility*>(receiverModule->getSubmodule("mobility")))
                {
                    delay = (sourceLutMobility->getDistance(destLutMobility->getLUTPositionY(), destLutMobility->getLUTPositionX(), 0)/299792458);
                }
                else if(SatelliteMobility *destSatMobility = dynamic_cast<SatelliteMobility*>(receiverModule->getSubmodule("mobility")))
                {
                    delay = ((destSatMobility->getDistance(sourceLutMobility->getLUTPositionY(), sourceLutMobility->getLUTPositionX(), 0)*1000)/299792458);
                }
            }

            if(SatelliteMobility *sourceSatMobility = dynamic_cast<SatelliteMobility*>(transmitterModule->getSubmodule("mobility")))
            {
                if(SatelliteMobility *destSatMobility = dynamic_cast<SatelliteMobility*>(receiverModule->getSubmodule("mobility")))
                {
                    delay = ((sourceSatMobility->getDistance(destSatMobility->getLatitude(), destSatMobility->getLongitude(), 550)*1000)/299792458);
                }
                else if(GroundStationMobility *destLutMobility = dynamic_cast<GroundStationMobility*>(receiverModule->getSubmodule("mobility")))
                {
                    delay = ((sourceSatMobility->getDistance(destLutMobility->getLUTPositionY(), destLutMobility->getLUTPositionX(), 0)*1000)/299792458);
                }
            }
            // we want to have a maximum PER product along the path,
            // but still minimize the hop count if the PER is negligible
            if(packetErrorRate==1){
                return minLinkWeight - log(1 - packetErrorRate);
            }
            else{
                return delay;
            }
        }
        else if (!strcmp(metric, "dataRate")) {
            cModule *transmitterInterfaceModule = link->sourceInterfaceInfo->interfaceEntry;
            IRadio *transmitterRadio = check_and_cast<IRadio *>(transmitterInterfaceModule->getSubmodule("radio"));
            const FlatTransmitterBase *transmitter = dynamic_cast<const FlatTransmitterBase *>(transmitterRadio->getTransmitter());
            double dataRate = transmitter ? transmitter->getBitrate().get() : 0;
            return dataRate != 0 ? 1 / dataRate : minLinkWeight;
        }
        else if (!strcmp(metric, "errorRate")) {
            // compute the packet error rate between the two interfaces using a dummy transmission
            const NetworkConfiguratorBase::InterfaceInfo *transmitterInterfaceInfo = link->sourceInterfaceInfo;
            const NetworkConfiguratorBase::InterfaceInfo *receiverInterfaceInfo = link->destinationInterfaceInfo;
            cModule *transmitterInterfaceModule = transmitterInterfaceInfo->interfaceEntry;
            cModule *receiverInterfaceModule = receiverInterfaceInfo->interfaceEntry;
            const IRadio *transmitterRadio = check_and_cast<IRadio *>(transmitterInterfaceModule->getSubmodule("radio"));
            const IRadio *receiverRadio = check_and_cast<IRadio *>(receiverInterfaceModule->getSubmodule("radio"));
            const IRadioMedium *medium = receiverRadio->getMedium();
            Packet *transmittedFrame = new Packet();
            auto byteCountChunk = makeShared<ByteCountChunk>(B(transmitterInterfaceInfo->interfaceEntry->getMtu()));
            transmittedFrame->insertAtBack(byteCountChunk);

            // KLUDGE:
            transmittedFrame->addTag<PacketProtocolTag>()->setProtocol(nullptr);
            check_and_cast<const Radio *>(transmitterRadio)->encapsulate(transmittedFrame);

            const ITransmission *transmission = transmitterRadio->getTransmitter()->createTransmission(transmitterRadio, transmittedFrame, simTime());
            const IArrival *arrival = medium->getPropagation()->computeArrival(transmission, receiverRadio->getAntenna()->getMobility());
            const IListening *listening = receiverRadio->getReceiver()->createListening(receiverRadio, arrival->getStartTime(), arrival->getEndTime(), arrival->getStartPosition(), arrival->getEndPosition());
            const INoise *noise = medium->getBackgroundNoise() != nullptr ? medium->getBackgroundNoise()->computeNoise(listening) : nullptr;
            const IReception *reception = medium->getAnalogModel()->computeReception(receiverRadio, transmission, arrival);
            const IInterference *interference = new Interference(noise, new std::vector<const IReception *>());
            const ISnir *snir = medium->getAnalogModel()->computeSNIR(reception, noise);
            const IReceiver *receiver = receiverRadio->getReceiver();
            bool isReceptionPossible = receiver->computeIsReceptionPossible(listening, reception, IRadioSignal::SIGNAL_PART_WHOLE);
            double packetErrorRate;
            if (!isReceptionPossible)
                packetErrorRate = 1;
            else {
                const IReceptionDecision *receptionDecision = new ReceptionDecision(reception, IRadioSignal::SIGNAL_PART_WHOLE, isReceptionPossible, true, true);
                const std::vector<const IReceptionDecision *> *receptionDecisions = new std::vector<const IReceptionDecision *> {receptionDecision};
                const IReceptionResult *receptionResult = receiver->computeReceptionResult(listening, reception, interference, snir, receptionDecisions);
                Packet *receivedFrame = const_cast<Packet *>(receptionResult->getPacket());
                packetErrorRate = receivedFrame->getTag<ErrorRateInd>()->getPacketErrorRate();
                delete receptionResult;
                delete receptionDecision;
            }
            delete snir;
            delete interference;
            delete reception;
            delete listening;
            delete arrival;
            delete transmission;
            delete transmittedFrame;
            return minLinkWeight - log(1 - packetErrorRate);
        }
#endif
        else
            throw cRuntimeError("Unknown metric");
    }
}
}
