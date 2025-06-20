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
    Ipv4RoutingTable::configureRouterId();
}

}
