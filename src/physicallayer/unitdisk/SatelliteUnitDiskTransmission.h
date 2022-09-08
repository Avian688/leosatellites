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

#ifndef OS3_PHYSICALLAYER_UNITDISK_SATELLITEUNITDISKTRANSMISSION_H_
#define OS3_PHYSICALLAYER_UNITDISK_SATELLITEUNITDISKTRANSMISSION_H_

#include "libnorad/ccoord.h"
#include "inet/physicallayer/wireless/unitdisk/UnitDiskTransmission.h"

namespace inet {
namespace physicallayer {

//-----------------------------------------------------
// Class: SatelliteUnitDiskTransmission
//
// To make sure that the propagation and path loss classes get the correct distance between two nodes (Satellite/Ground Station)
// this class was created. In contrast to the generic unit disk transmission class, the longitude, latitude and altitude of a
// specific transmitter is passed, allowing them to be mapped to a coordinate system, calculating the real distance between two nodes
// rather than the visual representation.
// Written by Aiden Valentine
//-----------------------------------------------------
class SatelliteUnitDiskTransmission : public UnitDiskTransmission
{
  protected:
    cCoordGeo longLatStartPosition;
    cCoordGeo longLatEndPosition;

  public:
    SatelliteUnitDiskTransmission(const IRadio *transmitter, const Packet *macFrame, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, m communicationRange, m interferenceRange, m detectionRange, cCoordGeo longLatStartPosition, cCoordGeo longLatEndPosition);
    virtual cCoordGeo getStartLongLatPosition() const { return longLatStartPosition; }
    virtual cCoordGeo getEndLongLatPosition() const { return longLatEndPosition; }
};

} /* namespace physicallayer */
} /* namespace inet */

#endif /* OS3_PHYSICALLAYER_UNITDISK_SATELLITEUNITDISKTRANSMISSION_H_ */
