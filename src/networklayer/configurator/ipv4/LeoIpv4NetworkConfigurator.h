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

#ifndef NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NETWORKCONFIGURATOR_H_
#define NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NETWORKCONFIGURATOR_H_

#include <algorithm>
#include <igraph.h>
#include <queue>
#include <tuple>
#include <inet/common/Topology.h>
#include "inet/networklayer/configurator/base/L3NetworkConfiguratorBase.h"
#include <inet/networklayer/ipv4/Ipv4InterfaceData.h>

#include "../../ipv4/LeoIpv4.h"
#include "../../../mobility/SatelliteMobility.h"
#include "../../../mobility/GroundStationMobility.h"

#include <chrono> //TODO remove if not using analysing runtime

namespace inet {
class INET_API LeoIpv4NetworkConfigurator : public L3NetworkConfiguratorBase {
protected:
    //typedef igraph_error_type_t igraph_error_t;
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override { throw cRuntimeError("this module doesn't handle messages, it runs only in initialize()"); }
    virtual void initialize(int stage) override;

    virtual double computeLinkWeight(Link *link, const char *metric, cXMLElement *parameters=nullptr) override;
    virtual double computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters=nullptr) override;
    virtual int getNextHopInterfaceID(cModule* sourceSatellite, cModule* nextHopSatellite);
    virtual void fillNextHopInterfaceMap();
    virtual cModule* getNodeModule(int nodeNumber);
    virtual void assignIDtoModules();
    virtual bool loadConfiguration(simtime_t currentInterval);
    virtual ~LeoIpv4NetworkConfigurator();
protected:
    //internal state
    Topology topology;
    bool loadFiles;
    std::unordered_map<int, cModule*> nodeModules;
    std::map<SatelliteMobility*, std::vector<SatelliteMobility*>> satelliteISLMobilityModules;
    std::map<cModule*, std::map<cModule*, int>> nextHopInterfaceMap;
    std::string networkName;
    std::string filePrefix;
    unsigned int numOfSats;
    unsigned int numOfGS;
    unsigned int numOfPlanes;
    unsigned int satPerPlane;
    unsigned int numOfISLs;
    const char* linkMetric;
    std::queue<std::tuple<int, int, double>> groundStationLinks;
    int numOfKPaths;

    simtime_t currentInterval;
    igraph_vector_int_t islVec;
public:
    virtual void establishInitialISLs();
    virtual void updateForwardingStates(simtime_t currentInterval);
    virtual void generateTopologyGraph(simtime_t currentInterval);
    virtual void addNextHopInterface(cModule* source, cModule* destination, int interfaceID);
    virtual void removeNextHopInterface(cModule* source, cModule* destination);
    virtual void addGSLinktoTopologyGraph(int gsNum, int destNum, double weight);
};
}
#endif /* NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NETWORKCONFIGURATOR_H_ */
