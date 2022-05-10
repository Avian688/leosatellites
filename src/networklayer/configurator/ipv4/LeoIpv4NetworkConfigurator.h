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
#include <inet/common/Topology.h>
#include <inet/networklayer/configurator/base/NetworkConfiguratorBase.h>

#include "../../ipv4/LeoIpv4.h"
namespace inet {

class INET_API LeoIpv4NetworkConfigurator : public NetworkConfiguratorBase {
protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override { throw cRuntimeError("this module doesn't handle messages, it runs only in initialize()"); }
    virtual void initialize(int stage) override;

    virtual double computeLinkWeight(Link *link, const char *metric, cXMLElement *parameters=nullptr) override;
    virtual double computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters=nullptr) override;
protected:
    //internal state
    Topology topology;
    const char* linkMetric;

public:
    virtual void updateForwardingStates();
};
}
#endif /* NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NETWORKCONFIGURATOR_H_ */
