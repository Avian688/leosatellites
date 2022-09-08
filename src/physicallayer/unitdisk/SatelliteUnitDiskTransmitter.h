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

#ifndef INET_PHYSICALLAYER_UNITDISK_SATELLITEUNITDISKTRANSMITTER_H_
#define INET_PHYSICALLAYER_UNITDISK_SATELLITEUNITDISKTRANSMITTER_H_

#include "libnorad/ccoord.h"
#include "inet/physicallayer/wireless/unitdisk/UnitDiskTransmitter.h"

namespace inet {
namespace physicallayer {

//-----------------------------------------------------
// Class: SatelliteUnitDiskTransmitter
//
// This class creates a SatelliteUnitDiskTransmission which stores the longitude, latitude and altitude of a given transmitter.
// These values are used within the path loss and propagation calculations so that the visual x, y and z values can be
// decoupled from their actual positions.
// Written by Aiden Valentine
//-----------------------------------------------------
class SatelliteUnitDiskTransmitter : public UnitDiskTransmitter
{
    public:
        virtual const ITransmission *createTransmission(const IRadio *radio, const Packet *packet, const simtime_t startTime) const override;
};

} /* namespace physicallayer */
} /* namespace inet */

#endif /* INET_PHYSICALLAYER_UNITDISK_SATELLITEUNITDISKTRANSMITTER_H_ */
