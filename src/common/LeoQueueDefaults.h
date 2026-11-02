// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef LEOSATELLITES_COMMON_LEOQUEUEDEFAULTS_H
#define LEOSATELLITES_COMMON_LEOQUEUEDEFAULTS_H

#include <omnetpp.h>

namespace inet {

// Call after interface creation, before finalizeParameters()/buildInside().
// The NED interface forwards this fallback via a default parameter pattern.
// Explicit queue settings win; no queue parameter needs runtime mutability.
inline void configureLeoQueueDefaults(omnetpp::cModule *interfaceModule, int packetCapacity)
{
    if (packetCapacity < -1)
        throw omnetpp::cRuntimeError("queueSize must be -1 (unlimited) or a non-negative packet count");
    if (interfaceModule->hasPar("defaultPacketCapacity") && !interfaceModule->par("defaultPacketCapacity").isSet())
        interfaceModule->par("defaultPacketCapacity").setIntValue(packetCapacity);
}

} // namespace inet

#endif
