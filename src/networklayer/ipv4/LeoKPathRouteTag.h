// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef LEOSATELLITES_LEOKPATHROUTETAG_H
#define LEOSATELLITES_LEOKPATHROUTETAG_H

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "inet/common/TagBase.h"
#include "omnetpp/ccommbuffer.h"

namespace inet {

// IPv4 header metadata survives PPP, which strips packet-level tags.
class LeoKPathRouteTag : public TagBase
{
  public:
    std::shared_ptr<const std::vector<int32_t>> nodes;
    int destinationNodeId = -1;

    LeoKPathRouteTag *dup() const override { return new LeoKPathRouteTag(*this); }

    void parsimPack(cCommBuffer *buffer) const override
    {
        TagBase::parsimPack(buffer);
        buffer->pack(destinationNodeId);
        buffer->pack(nodes ? static_cast<int>(nodes->size()) : 0);
        if (nodes)
            for (int32_t node : *nodes)
                buffer->pack(node);
    }

    void parsimUnpack(cCommBuffer *buffer) override
    {
        TagBase::parsimUnpack(buffer);
        buffer->unpack(destinationNodeId);
        int count;
        buffer->unpack(count);
        if (count < 0)
            throw cRuntimeError("Invalid pinned K-path length: %d", count);
        if (count == 0) {
            nodes.reset();
            return;
        }
        auto path = std::make_shared<std::vector<int32_t>>(count);
        for (int32_t& node : *path)
            buffer->unpack(node);
        nodes = std::move(path);
    }
};

} // namespace inet
#endif
