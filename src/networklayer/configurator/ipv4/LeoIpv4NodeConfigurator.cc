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

#include <inet/common/ModuleAccess.h>
#include <inet/common/lifecycle/ModuleOperations.h>
#include <inet/common/lifecycle/NodeStatus.h>
#include "LeoIpv4NodeConfigurator.h"

namespace inet {

Define_Module(LeoIpv4NodeConfigurator);

LeoIpv4NodeConfigurator::LeoIpv4NodeConfigurator() {
    nodeStatus = nullptr;
    interfaceTable = nullptr;
    routingTable = nullptr;     // potentially remove
    networkConfigurator = nullptr;
}

void LeoIpv4NodeConfigurator::initialize(int stage)
{
    cSimpleModule::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        cModule *node = getContainingNode(this);
        const char *networkConfiguratorPath = par("networkConfiguratorModule");
        nodeStatus = dynamic_cast<NodeStatus *>(node->getSubmodule("status"));
        interfaceTable = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);
        routingTable = getModuleFromPar<IIpv4RoutingTable>(par("routingTableModule"), this);

        if (!networkConfiguratorPath[0])
            networkConfigurator = nullptr;
        else {
            cModule *module = getModuleByPath(networkConfiguratorPath);
            if (!module)
                throw cRuntimeError("Configurator module '%s' not found (check the 'networkConfiguratorModule' parameter)", networkConfiguratorPath);
            networkConfigurator = check_and_cast<LeoIpv4NetworkConfigurator *>(module);
        }
    }
//    else if (stage == INITSTAGE_NETWORK_CONFIGURATION) {  //TODO is any of this needed?
//        if (!nodeStatus || nodeStatus->getState() == NodeStatus::UP)
//            prepareAllInterfaces();
//    }
//    else if (stage == INITSTAGE_STATIC_ROUTING) {
//        if ((!nodeStatus || nodeStatus->getState() == NodeStatus::UP) && networkConfigurator) {
//            //configureRoutingTable(); shouldnt need to configure routing table
//            cModule *node = getContainingNode(this);
//            // get a pointer to the host module and IInterfaceTable
//            node->subscribe(interfaceCreatedSignal, this);
//            node->subscribe(interfaceDeletedSignal, this);
//            node->subscribe(interfaceStateChangedSignal, this);
//        }
//    }
}

//TODO revise if this is needed
bool LeoIpv4NodeConfigurator::handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback)
{
    Enter_Method_Silent();
    int stage = operation->getCurrentStage();
    if (dynamic_cast<ModuleStartOperation *>(operation)) {
        if (static_cast<ModuleStartOperation::Stage>(stage) == ModuleStartOperation::STAGE_NETWORK_LAYER) {
            cModule *node = getContainingNode(this);
            node->subscribe(interfaceCreatedSignal, this);
            node->subscribe(interfaceDeletedSignal, this);
            node->subscribe(interfaceStateChangedSignal, this);
        }
    }
    else if (dynamic_cast<ModuleStopOperation *>(operation)) {
        if (static_cast<ModuleStopOperation::Stage>(stage) == ModuleStopOperation::STAGE_LOCAL) {
            cModule *node = getContainingNode(this);
            node->unsubscribe(interfaceCreatedSignal, this);
            node->unsubscribe(interfaceDeletedSignal, this);
            node->unsubscribe(interfaceStateChangedSignal, this);
        }
    }
    else if (dynamic_cast<ModuleCrashOperation *>(operation)) {
        cModule *node = getContainingNode(this);
        node->unsubscribe(interfaceCreatedSignal, this);
        node->unsubscribe(interfaceDeletedSignal, this);
        node->unsubscribe(interfaceStateChangedSignal, this);
    }
    else
        throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
    return true;
}

void LeoIpv4NodeConfigurator::receiveSignal(cComponent *source, simsignal_t signalID, cObject *obj, cObject *details)
{
    if (getSimulation()->getContextType() == CTX_INITIALIZE)
        return; // ignore notifications during initialize

    Enter_Method_Silent();
    printSignalBanner(signalID, obj, details);

    if (signalID == interfaceDeletedSignal) {
        // The RoutingTable deletes routing entries of interface
    }
}
}
