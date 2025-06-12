//
// Copyright (C) 2005 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package leosatellites.linklayer.ppp;

import inet.linklayer.contract.IPppInterface;
import inet.networklayer.common.NetworkInterface;
import inet.queueing.contract.IPacketQueue;
import inet.queueing.contract.ITrafficConditioner;
import inet.linklayer.ppp.Ppp;
import leosatellites.queueing.queue.DropTailQueueMutable;

//
// This module implements a IN-Network telemetry network interface.
//
module PppInterfaceMutable extends NetworkInterface like IPppInterface
{
    parameters:
        string interfaceTableModule;
        *.interfaceTableModule = default(absPath(this.interfaceTableModule));
    gates:
        input upperLayerIn;
        output upperLayerOut;
        inout phys @labels(PppFrame);
    submodules:
        ingressTC: <default("OmittedTrafficConditioner")> like ITrafficConditioner {
            parameters:
                @display("p=500,100");
        }
        egressTC: <default("OmittedTrafficConditioner")> like ITrafficConditioner {
            parameters:
                @display("p=300,100");
        }
        queue: DropTailQueueMutable {
            parameters:
                packetCapacity = default(10000);
                @display("p=300,200;q=l2queue");
        }
        ppp: Ppp {
            parameters:
                @display("p=500,300");
        }
    connections:
        upperLayerIn --> { @display("m=n"); } --> egressTC.in;
        egressTC.out --> queue.in;
        queue.out --> ppp.upperLayerIn;

        phys <--> { @display("m=s"); } <--> ppp.phys;

        ppp.upperLayerOut --> ingressTC.in;
        ingressTC.out --> { @display("m=n"); } --> upperLayerOut;
}

