// SPDX-License-Identifier: LGPL-3.0-or-later
#include <omnetpp.h>
#include <string>
#include "../../src/common/LeoQueueDefaults.h"

using namespace omnetpp;

// Construction regression: use the same hook as LeoChannelConstructor, with
// actual NED interfaces/queues. Deliberately do not initialize/transmit packets.
class QueueCompatibilityTest : public cSimpleModule
{
    void checkInterfaces()
    {
        const char *interfaces[] = {
            "leosatellites.linklayer.ppp.PppInterfaceMutable"
        };
        const char *defaults[] = {
            "leosatellites.queueing.queue.DropTailQueueMutable"
        };
        const std::string requested = par("expectedQueue").stdstringValue();
        for (int i = 0; i < 1; ++i) {
            for (int mode = 0; mode < 3; ++mode) {
                const std::string name = std::string(mode == 1 ? "explicitCapacity" : "defaultCapacity") + std::to_string(i);
                cModule *interfaceModule = cModuleType::get(interfaces[i])->create(name.c_str(), getParentModule());
                inet::configureLeoQueueDefaults(interfaceModule, mode == 2 ? -1 : 300);
                interfaceModule->finalizeParameters();
                interfaceModule->buildInside();
                cModule *queue = interfaceModule->getSubmodule("queue");
                const std::string expected = requested.empty() ? defaults[i] : requested;
                if (queue == nullptr || expected != queue->getNedTypeName())
                    throw cRuntimeError("Queue selection failed for %s", interfaces[i]);
                if (queue->hasPar("packetCapacity")) {
                    const int capacity = mode == 1 ? 73 : mode == 2 ? -1 : 300;
                    if (queue->par("packetCapacity").intValue() != capacity)
                        throw cRuntimeError("Queue capacity precedence failed for %s", queue->getFullPath().c_str());
                }
                // A compound queue's private configuration must not be overwritten.
                if (expected == "inet.queueing.queue.PriorityQueue") {
                    for (int j = 0; j < 2; ++j)
                        if (queue->getSubmodule("queue", j)->par("packetCapacity").intValue() != 17)
                            throw cRuntimeError("Inner queue capacity was overwritten");
                }
                interfaceModule->deleteModule();
            }
        }
    }

    void initialize() override
    {
        scheduleAt(SIMTIME_ZERO, new cMessage("initial construction"));
    }

    void handleMessage(cMessage *message) override
    {
        checkInterfaces();
        if (simTime() == SIMTIME_ZERO)
            scheduleAt(SimTime(1), message);
        else {
            delete message;
            EV_INFO << "PASS: queue selection and capacity at t=0 and t=1s\n";
        }
    }
};

Define_Module(QueueCompatibilityTest);
