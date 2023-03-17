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

#include "LeoIpv4RoutingTable.h"
#include "inet/networklayer/contract/IInterfaceTable.h"
#include "inet/networklayer/ipv4/Ipv4InterfaceData.h"
namespace inet {

Define_Module(LeoIpv4RoutingTable);

void LeoIpv4RoutingTable::initialize(int stage)
{
    Ipv4RoutingTable::initialize(stage);
}
void LeoIpv4RoutingTable::configureRouterId()
{
    if (routerId.isUnspecified()) {    // not yet configured
        const char *routerIdStr = par("routerId");
        if (!strcmp(routerIdStr, "auto")) {    // non-"auto" cases already handled earlier
            // choose highest interface address as routerId
            for (int i = 0; i < ift->getNumInterfaces(); ++i) {
                NetworkInterface *ie = ift->getInterface(i);
                if (!ie->isLoopback()) {
                    auto ipv4Data = ie->findProtocolData<Ipv4InterfaceData>();
                    if (ipv4Data && ipv4Data->getIPAddress().getInt() > routerId.getInt()) {
                        routerId = ipv4Data->getIPAddress();
                    }
                }
            }
        }
    }
    else {    // already configured
              // if there is no interface with routerId yet, assign it to the loopback address;
              // TODO find out if this is a good practice, in which situations it is useful etc.
        if (getInterfaceByAddress(routerId) == nullptr) {
            NetworkInterface *lo0 = CHK(ift->findFirstLoopbackInterface());
            auto ipv4Data = lo0->getProtocolDataForUpdate<Ipv4InterfaceData>();
            ipv4Data->setIPAddress(routerId);
            ipv4Data->setNetmask(Ipv4Address::ALLONES_ADDRESS);
        }
    }
}

}
