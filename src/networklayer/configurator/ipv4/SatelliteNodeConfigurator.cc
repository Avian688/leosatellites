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

#include "SatelliteNodeConfigurator.h"

namespace inet {

Define_Module(SatelliteNodeConfigurator);

void SatelliteNodeConfigurator::initialize(int stage)
{
    Ipv4NodeConfigurator::initialize(stage);
}

void SatelliteNodeConfigurator::prepareSatelliteLinkInterfaces()
{
    ASSERT(networkConfigurator);
    for (int i = 1; i < interfaceTable->getNumInterfaces(); i++)
        prepareInterface(interfaceTable->getInterface(i));
}

void SatelliteNodeConfigurator::prepareLastInterface()
{
    ASSERT(networkConfigurator);
    prepareInterface(interfaceTable->getInterface(interfaceTable->getNumInterfaces()-1));
}

void SatelliteNodeConfigurator::prepareInterface(InterfaceEntry *interfaceEntry)
{
    interfaceEntry->removeProtocolDataIfPresent<Ipv4InterfaceData>(); //TODO improve/remove
    Ipv4InterfaceData *interfaceData = interfaceEntry->addProtocolData<Ipv4InterfaceData>();
    if (interfaceEntry->isLoopback()) {
        // we may reconfigure later it to be the routerId
        interfaceData->setIPAddress(Ipv4Address::LOOPBACK_ADDRESS);
        interfaceData->setNetmask(Ipv4Address::LOOPBACK_NETMASK);
        interfaceData->setMetric(1);
    }
    else {
        auto datarate = interfaceEntry->getDatarate();
        // TODO: KLUDGE: how do we set the metric correctly for both wired and wireless interfaces even if datarate is unknown
        if (datarate == 0)
            interfaceData->setMetric(1);
        else
            // metric: some hints: OSPF cost (2e9/bps value), MS KB article Q299540, ...
            interfaceData->setMetric((int)ceil(2e9 / datarate));    // use OSPF cost as default
        if (interfaceEntry->isMulticast()) {
            interfaceData->joinMulticastGroup(Ipv4Address::ALL_HOSTS_MCAST);
            if (routingTable->isForwardingEnabled())
                interfaceData->joinMulticastGroup(Ipv4Address::ALL_ROUTERS_MCAST);
        }
    }
}

void SatelliteNodeConfigurator::configureAllInterfaces()
{
    ASSERT(networkConfigurator);
    for (int i = 0; i < interfaceTable->getNumInterfaces(); i++)
        networkConfigurator->configureInterface(interfaceTable->getInterface(i));

}

}
