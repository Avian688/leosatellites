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

#include "SatelliteUnitDiskTransmission.h"

namespace inet {
namespace physicallayer {

/**
 * The SatelliteUnitDiskTransmission class is a model representing the Unit Disk Transmission of a satellite. Unlike the INET
 * UnitDiskTransmission, this model encapsulates start and end position coordinates so that the propagation model can use the
 * actual long/lat/altitude positions of a ground-station/satellite over the 2D OMNeT++ canvas positions.
 */
SatelliteUnitDiskTransmission::SatelliteUnitDiskTransmission(const IRadio *transmitter, const Packet *macFrame, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, m communicationRange, m interferenceRange, m detectionRange, cCoordGeo longLatStartPosition, cCoordGeo longLatEndPosition) :
    UnitDiskTransmission(transmitter, macFrame, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, communicationRange, interferenceRange, detectionRange)
{
    this->longLatStartPosition = longLatStartPosition;
    this->longLatEndPosition = longLatEndPosition;
}
} /* namespace physicallayer */
} /* namespace inet */
