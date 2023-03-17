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

#ifndef OS3_PHYSICALLAYER_APSKRADIO_SATELLITEAPSKSCALARTRANSMITTER_H_
#define OS3_PHYSICALLAYER_APSKRADIO_SATELLITEAPSKSCALARTRANSMITTER_H_

#include "inet/physicallayer/wireless/apsk/packetlevel/ApskScalarTransmitter.h"

namespace inet {
namespace physicallayer {

//-----------------------------------------------------
// Class: SatelliteApskScalarTransmitter
//
// This class creates a SatelliteApskScalarTransmission which stores the longitude, latitude and altitude of a given transmitter.
// These values are used within the path loss and propagation calculations so that the visual x, y and z values can be
// decoupled from their actual positions. This model uses the APSK scalar model and is not used by the simulation model.
// It was implemented to provide the flexibility of using a more detailed model if required.
// Written by Aiden Valentine
//-----------------------------------------------------
class SatelliteApskScalarTransmitter : public ApskScalarTransmitter {
    public:
            virtual const ITransmission *createTransmission(const IRadio *radio, const Packet *packet, const simtime_t startTime) const override;
};

}
}
#endif /* OS3_PHYSICALLAYER_APSKRADIO_SATELLITEAPSKSCALARTRANSMITTER_H_ */
