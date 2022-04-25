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

#ifndef NETWORKLAYER_CONFIGURATOR_IPV4_SATELLITENODECONFIGURATOR_H_
#define NETWORKLAYER_CONFIGURATOR_IPV4_SATELLITENODECONFIGURATOR_H_

#include "inet/networklayer/configurator/ipv4/Ipv4NodeConfigurator.h"

#include "inet/common/IInterfaceRegistrationListener.h"

namespace inet {

class SatelliteNodeConfigurator : public Ipv4NodeConfigurator {
    public:
        virtual void prepareSatelliteLinkInterfaces();
        virtual void prepareLastInterface();
        virtual void prepareInterface(InterfaceEntry *interfaceEntry) override;
        virtual void configureAllInterfaces() override;
    protected:
        virtual void initialize(int stage) override;
};

} // namespace inet

#endif /* NETWORKLAYER_CONFIGURATOR_IPV4_SATELLITENODECONFIGURATOR_H_ */
