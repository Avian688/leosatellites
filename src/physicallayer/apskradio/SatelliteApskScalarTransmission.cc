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

#include "SatelliteApskScalarTransmission.h"

#include "inet/physicallayer/wireless/apsk/packetlevel/ApskScalarTransmission.h"
namespace inet {

namespace physicallayer {

SatelliteApskScalarTransmission::SatelliteApskScalarTransmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime, const simtime_t endTime, const simtime_t preambleDuration, const simtime_t headerDuration, const simtime_t dataDuration, const Coord startPosition, const Coord endPosition, const Quaternion startOrientation, const Quaternion endOrientation, const IModulation *modulation, b headerLength, b payloadLength, Hz centerFrequency, Hz bandwidth, bps bitrate, W power, cCoordGeo longLatStartPosition, cCoordGeo longLatEndPosition) :
    ApskScalarTransmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, modulation, headerLength, payloadLength, centerFrequency, bandwidth, bitrate, power)
{
    this->longLatStartPosition = longLatStartPosition;
    this->longLatEndPosition = longLatEndPosition;
}

} // namespace physicallayer

} // namespace inet

