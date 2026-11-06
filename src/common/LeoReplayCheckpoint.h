#ifndef LEOSATELLITES_COMMON_LEOREPLAYCHECKPOINT_H
#define LEOSATELLITES_COMMON_LEOREPLAYCHECKPOINT_H

#include <cstdint>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <set>
#include <vector>

namespace inet { namespace leoReplay {

// Exact simulation ticks, with the scale recorded rather than assumed.
struct GroundLink { int ground = -1, satellite = -1; int64_t delay = 0; };
struct Terminal {
    int satellite = -1, target = -1, old = -1;
    int64_t delay = 0, start = 0, reconnect = 0;
};
struct Checkpoint {
    int scale = 0;
    int64_t boundary = 0, nextUpdate = 0, nextAttachmentCheck = 0, lastAttachmentChange = 0;
    std::string signature;
    std::vector<int64_t> fixedDelays;
    std::vector<GroundLink> groundLinks;
    std::vector<Terminal> terminals;
};

inline void writeCheckpoint(std::ostream& out, const Checkpoint& state)
{
    out << "LEO_REPLAY_1\n" << state.scale << ' ' << state.boundary << ' ' << state.nextUpdate
        << ' ' << state.nextAttachmentCheck << ' ' << state.lastAttachmentChange << '\n' << std::quoted(state.signature) << '\n';
    out << state.fixedDelays.size() << '\n';
    for (auto delay : state.fixedDelays)
        out << delay << '\n';
    out << state.groundLinks.size() << '\n';
    for (const auto& link : state.groundLinks)
        out << link.ground << ' ' << link.satellite << ' ' << link.delay << '\n';
    out << state.terminals.size() << '\n';
    for (const auto& terminal : state.terminals)
        out << terminal.satellite << ' ' << terminal.delay << ' ' << terminal.target << ' '
            << terminal.old << ' ' << terminal.start << ' ' << terminal.reconnect << '\n';
    if (!out)
        throw std::runtime_error("Failed to write replay checkpoint");
}

inline Checkpoint readCheckpoint(std::istream& in, int scale, int64_t boundary,
                                 int64_t updateInterval, int64_t attachmentInterval,
                                 const std::string& signature, size_t fixedCount,
                                 int satelliteCount, int groundCount, int terminalCount)
{
    Checkpoint state;
    std::string magic;
    if (!(in >> magic >> state.scale >> state.boundary >> state.nextUpdate >> state.nextAttachmentCheck >> state.lastAttachmentChange
          >> std::quoted(state.signature)) || magic != "LEO_REPLAY_1")
        throw std::runtime_error("Missing, truncated or unsupported replay checkpoint");
    if (state.scale != scale || state.boundary != boundary || state.signature != signature)
        throw std::runtime_error("Replay checkpoint does not match time scale, boundary or topology configuration");
    if (state.lastAttachmentChange < 0 || state.lastAttachmentChange > boundary)
        throw std::runtime_error("Invalid last attachment change in replay checkpoint");
    if (state.nextUpdate < boundary || state.nextUpdate - boundary > updateInterval ||
        (attachmentInterval > 0 && (state.nextAttachmentCheck < boundary ||
                                   state.nextAttachmentCheck - boundary > attachmentInterval)))
        throw std::runtime_error("Invalid replay checkpoint update times");
    auto count = [&](size_t maximum) {
        size_t value;
        if (!(in >> value) || value > maximum)
            throw std::runtime_error("Invalid replay checkpoint dimensions");
        return value;
    };
    if (count(fixedCount) != fixedCount)
        throw std::runtime_error("Replay checkpoint ISL count mismatch");
    for (size_t i = 0; i < fixedCount; ++i) {
        int64_t delay;
        if (!(in >> delay) || delay < 0)
            throw std::runtime_error("Invalid replay checkpoint link delay");
        state.fixedDelays.push_back(delay);
    }
    const size_t links = count(static_cast<size_t>(groundCount) * satelliteCount);
    std::set<std::pair<int, int>> seen;
    for (size_t i = 0; i < links; ++i) {
        GroundLink link;
        if (!(in >> link.ground >> link.satellite >> link.delay) || link.ground < 0 || link.ground >= groundCount ||
            link.satellite < 0 || link.satellite >= satelliteCount || link.delay < 0 ||
            !seen.emplace(link.ground, link.satellite).second)
            throw std::runtime_error("Invalid or duplicate ground link in replay checkpoint");
        state.groundLinks.push_back(link);
    }
    if (count(terminalCount) != static_cast<size_t>(terminalCount))
        throw std::runtime_error("Replay checkpoint terminal count mismatch");
    for (int i = 0; i < terminalCount; ++i) {
        Terminal terminal;
        if (!(in >> terminal.satellite >> terminal.delay >> terminal.target >> terminal.old >> terminal.start >> terminal.reconnect) ||
            terminal.satellite < -1 || terminal.satellite >= satelliteCount || terminal.target < -1 ||
            terminal.target >= satelliteCount || terminal.old < -1 || terminal.old >= satelliteCount || terminal.delay < 0 ||
            (terminal.target >= 0 && (terminal.satellite >= 0 || terminal.reconnect < boundary || terminal.start > boundary)))
            throw std::runtime_error("Invalid terminal handover in replay checkpoint");
        state.terminals.push_back(terminal);
    }
    std::string extra;
    if (in >> extra)
        throw std::runtime_error("Unexpected trailing data in replay checkpoint");
    return state;
}

} }
#endif
