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

#ifndef NETWORKLAYER_IPV4_LEOIPV4_H_
#define NETWORKLAYER_IPV4_LEOIPV4_H_

#include <inet/networklayer/ipv4/Ipv4.h>
#include <inet/networklayer/common/NextHopAddressTag_m.h>
#include <inet/linklayer/common/InterfaceTag_m.h>

namespace inet {

class LeoIpv4NetworkConfigurator;

class INET_API LeoIpv4 : public Ipv4{
protected:
    virtual void initialize(int stage) override;
    virtual void routeUnicastPacket(Packet *packet) override;
    virtual void stop() override;
    std::map<uint32_t,uint32_t> nextHops;   //Destination Address (int format) -> Interface ID
    std::map<std::string,std::string> nextHopsStr;   //Destination Address (int format) -> Interface ID
    std::map<int, std::map<int,int>> kNextHops;
public:
    void addKNextHop(int k, int destinationNode, int nextInterfaceID);
    void addNextHop(uint32_t destinationAddr, uint32_t nextInterfaceID);
    void addNextHopStr(std::string destinationAddr, std::string nextInterfaceID);
    void clearNextHops();
    LeoIpv4();
    virtual ~LeoIpv4();

protected:
    LeoIpv4NetworkConfigurator* configurator;
};
}
#endif
