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

#include "LeoIpv4NetworkConfigurator.h"

namespace inet {
Define_Module(LeoIpv4NetworkConfigurator);

void LeoIpv4NetworkConfigurator::initialize(int stage)
{
    if(stage == INITSTAGE_LOCAL){
        minLinkWeight = par("minLinkWeight");
        configureIsolatedNetworksSeparatly = par("configureIsolatedNetworksSeparatly").boolValue();
    }
}

void LeoIpv4NetworkConfigurator::updateForwardingStates()
{
    std::string lm = "delay";
    linkMetric = lm.c_str(); // TODO update to have more metrics

    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        node->interfaceInfos.clear();
        dynamic_cast<LeoIpv4 *>(node->module->getModuleByPath(".ipv4.ip"))->clearNextHops();
    }
    topology.linkInfos.clear();
    topology.interfaceInfos.clear();
    topology.clear();
    extractTopology(topology);
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *node = (Node *)topology.getNode(i);
        node->setWeight(0);
    }
    for (int i = 0; i < topology.getNumNodes(); i++) {
            Node *node = (Node *)topology.getNode(i);
            node->setWeight(0);
            for (int j = 0; j < node->getNumInLinks(); j++) {
                Link *link = (Link *)node->getLinkIn(j);
                //std::cout << "\n Metric before method: " << linkMetric << endl;
                double weight = computeLinkWeight(link, linkMetric, nullptr);
                EV_DEBUG << "Setting link weight, link = " << link << ", weight = " << weight << endl;
                //std::cout << "Setting link weight, link = " << link << ", weight = " << weight << endl;
                link->setWeight(weight);
            }
    }
    //std::cout << "\nCalculating shortest path algorithm for " << topology.getNumNodes() << " nodes..." << endl;
    for (int i = 0; i < topology.getNumNodes(); i++) {
        Node *sourceNode = (Node *)topology.getNode(i);
        //std::cout << "\n\nSource Node: " << sourceNode->module->getFullName() << endl;
        topology.calculateWeightedSingleShortestPathsTo(sourceNode);
        for (int j = 0; j < topology.getNumNodes(); j++) {
            // extract destination
            Node *destinationNode = (Node *)topology.getNode(j);
            //std::cout << "\nDestination Node: " << destinationNode->module->getFullName() << endl;
            if (sourceNode == destinationNode)
                continue;
            if (destinationNode->getNumPaths() == 0)
                continue;
            Node *node = destinationNode;
            Link *link = nullptr;
            InterfaceInfo *nextHopInterfaceInfo = nullptr;
            InterfaceInfo *currentInterfaceInfo = nullptr;
            //InterfaceInfo *destinationInterfaceInfo = nullptr;
            while (node != sourceNode) {
                link = (Link *)node->getPath(0);
                if (node != sourceNode && link->sourceInterfaceInfo){
                    nextHopInterfaceInfo = static_cast<InterfaceInfo *>(link->sourceInterfaceInfo);
                    currentInterfaceInfo = static_cast<InterfaceInfo *>(link->destinationInterfaceInfo);
                }
                //if(node == destinationNode && link->sourceInterfaceInfo){
                 //   destinationInterfaceInfo = static_cast<InterfaceInfo *>(link->sourceInterfaceInfo);
                //}
                node = (Node *)node->getPath(0)->getRemoteNode();

            }
            //std::cout << "Source Node: " << sourceNode->module->getFullName() << " Dest Node: " << destinationNode->module->getFullName() << endl;
            //std::cout << "Current Interface: " << currentInterfaceInfo->interfaceEntry->getIpv4Address().str() << "<-> Next Hop Interface: " << nextHopInterfaceInfo->interfaceEntry->getIpv4Address().str() << endl;
            //std::cout << "Dest Interface: " << destinationInterfaceInfo->interfaceEntry->getIpv4Address().str() << endl;
            LeoIpv4* ipv4Mod = dynamic_cast<LeoIpv4 *>(sourceNode->module->getModuleByPath(".ipv4.ip"));
            //TODO add hop for ALL dest interfaces.
            for (size_t j = 0; j < destinationNode->interfaceInfos.size(); j++) {
                InterfaceInfo *destinationInterfaceInfo = static_cast<InterfaceInfo *>(destinationNode->interfaceInfos[j]);
                InterfaceEntry *destinationInterfaceEntry = destinationInterfaceInfo->interfaceEntry;
                if (!destinationInterfaceEntry->isLoopback()){
                    ipv4Mod->addNextHop(destinationInterfaceInfo->interfaceEntry->getIpv4Address().getInt(),currentInterfaceInfo->interfaceEntry->getInterfaceId());
                    //ipv4Mod->addNextHopStr(destinationInterfaceInfo->interfaceEntry->getIpv4Address().str(),currentInterfaceInfo->interfaceEntry->getInterfaceId());
                }
            }
            //add entry for EACH destination interface
        }
    }
    //std::cout << "\nFinished shortest path algorithm." << endl;
}

double LeoIpv4NetworkConfigurator::computeLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
        return computeWiredLinkWeight(link, metric, nullptr);
}

double LeoIpv4NetworkConfigurator::computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
    //std::cout << "\n Metric: " << metric << endl;
    Topology::LinkOut *linkOut = static_cast<Topology::LinkOut *>(static_cast<Topology::Link *>(link));
    if (!strcmp(metric, "hopCount"))
        return 1;
    else if (!strcmp(metric, "delay")) {
        cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLocalGate()->findTransmissionChannel());
        if (transmissionChannel != nullptr)
            return transmissionChannel->getDelay().dbl();
        else
            return minLinkWeight;
    }
    else if (!strcmp(metric, "dataRate")) {
        cChannel *transmissionChannel = linkOut->getLocalGate()->findTransmissionChannel();
        if (transmissionChannel != nullptr) {
            double dataRate = transmissionChannel->getNominalDatarate();
            return dataRate != 0 ? 1 / dataRate : minLinkWeight;
        }
        else
            return minLinkWeight;
    }
    else if (!strcmp(metric, "errorRate")) {
        cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLocalGate()->findTransmissionChannel());
        if (transmissionChannel != nullptr) {
            InterfaceInfo *sourceInterfaceInfo = link->sourceInterfaceInfo;
            double bitErrorRate = transmissionChannel->getBitErrorRate();
            double packetErrorRate = 1.0 - pow(1.0 - bitErrorRate, sourceInterfaceInfo->interfaceEntry->getMtu());
            return minLinkWeight - log(1 - packetErrorRate);
        }
        else
            return minLinkWeight;
    }
    else
        throw cRuntimeError("Unknown metric");
}
// TODO If we set IPAddress in LeoChannelConstructor, do we need this? doubt it
//void LeoIpv4NetworkConfigurator::configureInterface(InterfaceInfo *interfaceInfo)
//{
//    EV_DETAIL << "Configuring network interface " << interfaceInfo->getFullPath() << ".\n";
//    InterfaceEntry *interfaceEntry = interfaceInfo->interfaceEntry;
//    Ipv4InterfaceData *interfaceData = interfaceEntry->getProtocolData<Ipv4InterfaceData>();
//    if (interfaceInfo->mtu != -1)
//        interfaceEntry->setMtu(interfaceInfo->mtu);
//    if (interfaceInfo->metric != -1)
//        interfaceData->setMetric(interfaceInfo->metric);
//    if (assignAddressesParameter) {
//        interfaceData->setIPAddress(Ipv4Address(interfaceInfo->address));
//        interfaceData->setNetmask(Ipv4Address(interfaceInfo->netmask));
//    }
//    // TODO: should we leave joined multicast groups first?
//    for (auto & multicastGroup : interfaceInfo->multicastGroups)
//        interfaceData->joinMulticastGroup(multicastGroup);
//}

}

