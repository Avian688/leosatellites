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

#include "SatelliteApskScalarTransmitter.h"

#include "SatelliteApskScalarTransmission.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/apsk/packetlevel/ApskPhyHeader_m.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/RadioControlInfo_m.h"

#include "mobility/LUTMotionMobility.h"
#include "mobility/SatSGP4Mobility.h"
namespace inet {

namespace physicallayer {

Define_Module(SatelliteApskScalarTransmitter);

const ITransmission *SatelliteApskScalarTransmitter::createTransmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime) const
{
    auto phyHeader = packet->peekAtFront<ApskPhyHeader>();
    ASSERT(phyHeader->getChunkLength() == headerLength);
    auto dataLength = packet->getTotalLength() - phyHeader->getChunkLength();
    W transmissionPower = computeTransmissionPower(packet);
    Hz transmissionCenterFrequency = computeCenterFrequency(packet);
    Hz transmissionBandwidth = computeBandwidth(packet);
    bps transmissionBitrate = computeTransmissionDataBitrate(packet);
    const simtime_t headerDuration = b(headerLength).get() / bps(transmissionBitrate).get();
    const simtime_t dataDuration = b(dataLength).get() / bps(transmissionBitrate).get();
    const simtime_t duration = preambleDuration + headerDuration + dataDuration;
    const simtime_t endTime = startTime + duration;
    IMobility *mobility = transmitter->getAntenna()->getMobility();
    const Coord startPosition = mobility->getCurrentPosition();
    const Coord endPosition = mobility->getCurrentPosition();

    auto longLatStartPosition = cCoordGeo();
    auto longLatEndPosition = cCoordGeo();

    if (SatSGP4Mobility *sgp4Mobility = dynamic_cast<SatSGP4Mobility *>(mobility)) { //The node is a satellite
        longLatStartPosition = cCoordGeo(sgp4Mobility->getLatitude(), sgp4Mobility->getLongitude(), sgp4Mobility->getAltitude());
        longLatEndPosition = cCoordGeo(sgp4Mobility->getLatitude(), sgp4Mobility->getLongitude(), sgp4Mobility->getAltitude());
    } else if (LUTMotionMobility *lutMobility = dynamic_cast<LUTMotionMobility *>(mobility)) {  //The node transmitter is a ground station
        longLatStartPosition = cCoordGeo(lutMobility->getLUTPositionY(), lutMobility->getLUTPositionX(), 0);
        longLatEndPosition = cCoordGeo(lutMobility->getLUTPositionX(), lutMobility->getLUTPositionY(), 0);
    } else {  //other
        longLatStartPosition = cCoordGeo(0, 0, 0);
        longLatEndPosition = cCoordGeo(0, 0, 0);
    }
    const Quaternion startOrientation = mobility->getCurrentAngularPosition();
    const Quaternion endOrientation = mobility->getCurrentAngularPosition();
    return new SatelliteApskScalarTransmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration, startPosition, endPosition, startOrientation, endOrientation, modulation, headerLength, dataLength, transmissionCenterFrequency, transmissionBandwidth, transmissionBitrate, transmissionPower, longLatStartPosition, longLatEndPosition);
}

} // namespace physicallayer

} // namespace inet

