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

#ifndef NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NODECONFIGURATOR_H_
#define NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NODECONFIGURATOR_H_

#include <inet/common/INETDefs.h>
#include <inet/common/lifecycle/ILifecycle.h>
#include <inet/common/lifecycle/NodeStatus.h>
#include <inet/networklayer/contract/IInterfaceTable.h>
#include <inet/networklayer/ipv4/IIpv4RoutingTable.h>

#include "LeoIpv4NetworkConfigurator.h"
#include "LeoIpv4NodeConfigurator.h"
namespace inet {
class INET_API LeoIpv4NodeConfigurator : public cSimpleModule, public ILifecycle, protected cListener {
protected:
   NodeStatus *nodeStatus;
   IInterfaceTable *interfaceTable;
   IIpv4RoutingTable *routingTable;
   LeoIpv4NetworkConfigurator *networkConfigurator;
public:
    LeoIpv4NodeConfigurator();

protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void handleMessage(cMessage *msg) override { throw cRuntimeError("this module doesn't handle messages, it runs only in initialize()"); }
    virtual void initialize(int stage) override;
    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
    /**
     * Called by the signal handler whenever a change of a category
     * occurs to which this client has subscribed.
     */
    virtual void receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details) override;
};
}
#endif /* NETWORKLAYER_CONFIGURATOR_IPV4_LEOIPV4NODECONFIGURATOR_H_ */
