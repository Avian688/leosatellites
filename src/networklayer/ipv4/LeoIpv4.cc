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
        WATCH_MAP(nextHops);
        //WATCH_MAP(nextHopsStr);
    }
}

void LeoIpv4::addNextHop(uint32 destinationAddr, uint32 nextInterfaceID){
    nextHops[destinationAddr] = nextInterfaceID;
}

void LeoIpv4::clearNextHops(){
    nextHops.clear();
}

void LeoIpv4::routeUnicastPacket(Packet *packet)
{
    //std::cout << "Routing Unicast Packet: " << packet->str() << endl;
    const InterfaceEntry *fromIE = getSourceInterface(packet);
    const InterfaceEntry *destIE = getDestInterface(packet);
    bool hopFound = false;
    // Initial Syn packet does not have either source or dest interface set
    Ipv4Address nextHopAddress = getNextHop(packet);

    const auto& ipv4Header = packet->peekAtFront<Ipv4Header>();
    Ipv4Address destAddr = ipv4Header->getDestAddress();
    EV_INFO << "Routing " << packet << " with destination = " << destAddr << ", ";
    //std::cout << "Routing " << packet << " with destination = " << destAddr << ", ";
    // if output port was explicitly requested, use that, otherwise use Ipv4 routing
    if (destIE) {
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
    }
    else {
        // use Ipv4 routing (lookup in routing table)
        //std::cout << "\nFinding best matching route for: " << destAddr.str() << endl;
        //const Ipv4Route *re = rt->findBestMatchingRoute(destAddr);
        int interfaceID = nextHops[destAddr.getInt()];
        if (interfaceID) {
            //std::cout << "Adding route with the following dest ID: " << interfaceID << endl;
            //std::cout << "Gateway: " << re->getGateway() << endl;
            packet->addTagIfAbsent<InterfaceReq>()->setInterfaceId(interfaceID);
            hopFound = true;
            //packet->addTagIfAbsent<NextHopAddressReq>()->setNextHopAddress(re->getGateway());
        }
        else{
            std::cout << "Interface ID not found!" << endl;
        }
    }

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
}
}
