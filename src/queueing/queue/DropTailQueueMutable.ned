//
// Copyright (C) 2020 OpenSim Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package leosatellites.queueing.queue;

import inet.queueing.queue.PacketQueue;
//
// This module is a limited packet queue which drops packets at the tail of the queue.
//
simple DropTailQueueMutable extends PacketQueue
{
    parameters:
        packetCapacity @mutable = default(100);
        dropperClass = default("inet::queueing::PacketAtCollectionEndDropper");
}
