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

#ifndef NETWORKLAYER_IPV4_LEOIPV4ROUTINGTABLE_H_
#define NETWORKLAYER_IPV4_LEOIPV4ROUTINGTABLE_H_

#include "inet/networklayer/ipv4/Ipv4RoutingTable.h"
namespace inet {

class INET_API LeoIpv4RoutingTable : public Ipv4RoutingTable {
public:
    virtual void configureRouterId() override; //allow LeoChannelConstructor to configure router id

protected:
    virtual void initialize(int stage) override;
};
}
#endif /* NETWORKLAYER_IPV4_LEOIPV4ROUTINGTABLE_H_ */
