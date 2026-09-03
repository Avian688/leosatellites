//
// ICMP identifier encoding used by KShortestPathPingApp. Keeping the path
// selection in the ordinary echo identifier lets requests and replies select
// the same saved path without adding a simulated source-routing header.
//

#ifndef APPLICATIONS_PINGAPP_KSHORTESTPATHPINGIDENTIFIER_H_
#define APPLICATIONS_PINGAPP_KSHORTESTPATHPINGIDENTIFIER_H_

namespace inet {

constexpr int K_PATH_PING_IDENTIFIER_PREFIX = 0x4b00;
constexpr int K_PATH_PING_IDENTIFIER_MASK = 0xff00;
constexpr int K_PATH_PING_MAX_GROUPS = 16;
constexpr int K_PATH_PING_MAX_PATHS = 16;

inline int makeKPathPingIdentifier(int pathGroup, int pathIndex)
{
    return K_PATH_PING_IDENTIFIER_PREFIX | (pathGroup << 4) | (pathIndex - 1);
}

inline bool decodeKPathPingIdentifier(int identifier, int& pathGroup, int& pathIndex)
{
    if ((identifier & K_PATH_PING_IDENTIFIER_MASK) != K_PATH_PING_IDENTIFIER_PREFIX)
        return false;
    pathGroup = (identifier >> 4) & 0xf;
    pathIndex = (identifier & 0xf) + 1;
    return true;
}

} // namespace inet

#endif
