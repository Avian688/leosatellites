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

#ifndef NETWORKLAYER_IPV4_LEOIPV4NETWORKLAYER_H_
#define NETWORKLAYER_IPV4_LEOIPV4NETWORKLAYER_H_

#include "inet/common/INETDefs.h"

namespace inet {

class INET_API LeoIpv4NetworkLayer : public cModule
{
  protected:
    virtual void refreshDisplay() const override;
    virtual void updateDisplayString() const;
};

} // namespace inet

#endif // ifndef NETWORKLAYER_IPV4_LEOIPV4NETWORKLAYER_H_

