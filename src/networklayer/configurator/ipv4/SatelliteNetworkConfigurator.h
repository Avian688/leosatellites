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

#ifndef LEOSATELLITES_SATELLITENETWORKCONFIGURATOR_H_
#define LEOSATELLITES_SATELLITENETWORKCONFIGURATOR_H_

#include "inet/networklayer/configurator/ipv4/Ipv4NetworkConfigurator.h"
#include "inet/networklayer/configurator/base/NetworkConfiguratorBase.h"
#include "inet/common/packet/chunk/ByteCountChunk.h"

namespace inet {

/**-----------------------------------------------------
 * Class: SatelliteNetworkConfigurator
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
 * -----------------------------------------------------
 * Written by Aiden Valentine
 */
class SatelliteNetworkConfigurator : public Ipv4NetworkConfigurator{
    public:
        virtual void reinvokeConfigurator(Topology& topology, cXMLElement *autorouteElement);
        void configureRoutingTable(Node *node);
        virtual void configureAllRoutingTables() override;
        virtual void configureRoutingTable(IIpv4RoutingTable *routingTable) override;
        virtual void addStaticRoutes(Topology& topology, cXMLElement *element);
        virtual ~SatelliteNetworkConfigurator(){};
    protected:
        simtime_t timerInterval;
        cMessage * timer = nullptr;
        bool enableInterSatelliteLinksParameter = true;
        virtual void extractTopology(Topology& topology);
        virtual void extractWiredNeighbors(Topology& topology, Topology::Link *linkOut, LinkInfo *linkInfo, std::map<int, NetworkInterface *>& interfacesSeen, std::vector<Node *>& nodesVisited);
        virtual void extractWirelessNeighbors(Topology& topology, const char *wirelessId, LinkInfo *linkInfo, std::map<int, NetworkInterface *>& interfacesSeen, std::vector<Node *>& nodesVisited);
        virtual void extractDeviceNeighbors(Topology& topology, Node *node, LinkInfo *linkInfo, std::map<int, NetworkInterface *>& interfacesSeen, std::vector<Node *>& deviceNodesVisited);
        virtual double computeWirelessLinkWeight(Link *link, const char *metric, cXMLElement *parameters) override;
        virtual void handleMessage(cMessage *msg) override;
        virtual void initialize(int stage) override;


        //virtual void extractTopology(Topology& topology);



};
}
#endif /* OS3_NETWORKLAYER_CONFIGURATOR_IPV4_SATELLITENETWORKCONFIGURATOR_H_ */
