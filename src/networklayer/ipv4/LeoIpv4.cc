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
#include "LeoIpv4.h"

#include <algorithm>

#include "../configurator/ipv4/LeoIpv4NetworkConfigurator.h"

namespace inet {

Define_Module(LeoIpv4);

LeoIpv4::LeoIpv4()
{
}

LeoIpv4::~LeoIpv4()
{
//    for (auto it : nextHops)
//        delete it.second;
}

void LeoIpv4::initialize(int stage)
{
    Ipv4::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        configurator = dynamic_cast<LeoIpv4NetworkConfigurator *>(getParentModule()->getParentModule()->getParentModule()->getSubmodule("configurator"));
    }
}

void LeoIpv4::setNodeId(int id)
{
    nodeId = id;
}
void LeoIpv4::addKNextHop(int k, int destNode, int nextInterfaceID)
{
    if (k == 1) {
        if (destNode >= static_cast<int>(primaryNextHopInterfaces.size()))
            primaryNextHopInterfaces.resize(destNode + 1, 0);
        primaryNextHopInterfaces[destNode] = nextInterfaceID;
    }
    else {
        kNextHops[k][destNode] = nextInterfaceID;
    }
}

void LeoIpv4::addNextHop(uint32_t destinationAddr, uint32_t nextInterfaceID)
{
    nextHops[destinationAddr] = nextInterfaceID;
}

void LeoIpv4::clearNextHops(){
    nextHops.clear();
    kNextHops.clear();
    std::fill(primaryNextHopInterfaces.begin(), primaryNextHopInterfaces.end(), 0);
}

void LeoIpv4::routeUnicastPacket(Packet *packet)
{
    //std::cout << "Routing Unicast Packet: " << packet->str() << endl;
    const NetworkInterface *fromIE = getSourceInterface(packet);
    const NetworkInterface *destIE = getDestInterface(packet);
    bool hopFound = false;
    // Initial Syn packet does not have either source or dest interface set
    Ipv4Address nextHopAddress = getNextHop(packet);

    const auto& ipv4Header = packet->peekAtFront<Ipv4Header>();
    Ipv4Address destAddr = ipv4Header->getDestAddress();
    EV_INFO << "Routing " << packet << " with destination = " << destAddr << ", ";
    //std::cout << "Routing " << packet << " with destination = " << destAddr << ", ";
    // if output port was explicitly requested, use that, otherwise use Ipv4 routing
    //if (destIE) {
        if (destIE != nullptr)
            EV_DETAIL << "using manually specified output interface " << destIE->getInterfaceName() << "\n";
        // and nextHopAddr remains unspecified
        //if (!nextHopAddress.isUnspecified()) {
            // do nothing, next hop address already specified
        //}
//        // special case ICMP reply
//        else if (destIE->isBroadcast()) {
//            // if the interface is broadcast we must search the next hop
//            const Ipv4Route *re = rt->findBestMatchingRoute(destAddr);
//            int interfaceID = nextHops[destAddr.getInt()];
//            if (interfaceID && interfaceID == destIE->getInterfaceId()) {
//                packet->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(re->getGateway());
//            }
//        }
    //}
    //else {
        // use Ipv4 routing (lookup in routing table)
    int modId = configurator->getModuleIdFromIpAddress(destAddr.getInt()); //TODO check if IP address is end point, if is, get ground station IP

    const int currentNodeType = configurator->getNodeTypeCode(nodeId);
    const int destinationNodeType = configurator->getNodeTypeCode(modId);
    int interfaceID = (modId >= 0 && modId < static_cast<int>(primaryNextHopInterfaces.size())) ? primaryNextHopInterfaces[modId] : 0;

    if (destinationNodeType == 2 && modId >= 0) {
        const int attachedNodeId = configurator->getGroundStationFromEndPoint(modId);
        if (nodeId == attachedNodeId) {
            interfaceID = configurator->getEndpointAttachmentInterfaceId(modId);
        }
        else if (currentNodeType == 2) {
            interfaceID = configurator->getEndpointUplinkInterfaceId(nodeId);
        }
        else {
            interfaceID = (attachedNodeId >= 0 && attachedNodeId < static_cast<int>(primaryNextHopInterfaces.size())) ? primaryNextHopInterfaces[attachedNodeId] : 0;
        }
    }
    else if (interfaceID <= 0 && currentNodeType == 2) {
        interfaceID = configurator->getEndpointUplinkInterfaceId(nodeId);
    }

    if (interfaceID > 0) {
        packet->addTagIfAbsent<InterfaceReq>()->setInterfaceId(interfaceID);
        hopFound = true;
    }
    else{
        EV_WARN << "\nInterface ID not found!: ID " << interfaceID << " at time: " << simTime() << endl;
        std::cout << "\nInterface ID not found!: Dest Addr " << destAddr.getInt() << " Mod ID " << modId << " ID " << interfaceID << " at time: " << simTime() << endl;
    }
    //}

    if (!hopFound) {    // no route found
        EV_WARN << "unroutable, sending ICMP_DESTINATION_UNREACHABLE, dropping packet\n";
        std::cout << "unroutable, sending ICMP_DESTINATION_UNREACHABLE, dropping packet\n";
        numUnroutable++;
        PacketDropDetails details;
        details.setReason(NO_ROUTE_FOUND);
        emit(packetDroppedSignal, packet, &details);
        sendIcmpError(packet, fromIE ? fromIE->getInterfaceId() : -1, ICMP_DESTINATION_UNREACHABLE, 0);
    }
    else {    // fragment and send
        //std::cout << "\n Packet being routed!!" << endl;
        if (fromIE != nullptr) {
            if (datagramForwardHook(packet) != INetfilter::IHook::ACCEPT)
                return;
        }

        routeUnicastPacketFinish(packet);
    }
}

void LeoIpv4::stop()
{
    Ipv4::stop();
    nextHops.clear();
    std::fill(primaryNextHopInterfaces.begin(), primaryNextHopInterfaces.end(), 0);
}
}
