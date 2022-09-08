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

#include "SatelliteUnitDiskTransmitter.h"

#include "SatelliteUnitDiskTransmission.h"
#include "inet/mobility/contract/IMobility.h"
#include "inet/physicallayer/wireless/common/contract/packetlevel/SignalTag_m.h"
#include "inet/physicallayer/wireless/unitdisk/UnitDiskPhyHeader_m.h"
#include "inet/physicallayer/wireless/unitdisk/UnitDiskTransmission.h"
#include "inet/physicallayer/wireless/unitdisk/UnitDiskTransmitter.h"
#include "mobility/LUTMotionMobility.h"
#include "mobility/SatSGP4Mobility.h"
#include "../../mobility/GroundStationMobility.h"
#include "../../mobility/SatelliteMobility.h"
namespace inet {
namespace physicallayer {

Define_Module(SatelliteUnitDiskTransmitter);

/**
 * createTransmission method which is called when a transmission is made between a source and destination. The transmitter must
 * create a transmission model object which encapsulates all of the transmission details. The coordinates are encapsulated to the
 * transmission model so that the required INET models can use the real coordinates to calculate distance over the 2D OMNeT++ positions.
 */
const ITransmission *SatelliteUnitDiskTransmitter::createTransmission(const IRadio *transmitter, const Packet *packet, const simtime_t startTime) const
{
    auto phyHeader = packet->peekAtFront<UnitDiskPhyHeader>();
    auto dataLength = packet->getTotalLength() - phyHeader->getChunkLength();
    auto signalBitrateReq = const_cast<Packet *>(packet)->findTag<SignalBitrateReq>();
    auto transmissionBitrate = signalBitrateReq != nullptr ? signalBitrateReq->getDataBitrate() : bitrate;
    auto headerDuration = b(headerLength).get() / bps(transmissionBitrate).get();
    auto dataDuration = b(dataLength).get() / bps(transmissionBitrate).get();
    auto duration = preambleDuration + headerDuration + dataDuration;
    auto endTime = startTime + duration;
    auto mobility = transmitter->getAntenna()->getMobility();

    auto startPosition = mobility->getCurrentPosition();
    auto endPosition = mobility->getCurrentPosition();

    auto longLatStartPosition = cCoordGeo();
    auto longLatEndPosition = cCoordGeo();

    auto startOrientation = mobility->getCurrentAngularPosition();
    auto endOrientation = mobility->getCurrentAngularPosition();

    //Make sure that the transmitter is either a satellite or ground station.
    if (SatelliteMobility *sgp4Mobility = dynamic_cast<SatelliteMobility *>(mobility)) { //The node is a satellite
        longLatStartPosition = cCoordGeo(sgp4Mobility->getLatitude(), sgp4Mobility->getLongitude(), sgp4Mobility->getAltitude());
        longLatEndPosition = cCoordGeo(sgp4Mobility->getLatitude(), sgp4Mobility->getLongitude(), sgp4Mobility->getAltitude());
    } else if (GroundStationMobility *lutMobility = dynamic_cast<GroundStationMobility *>(mobility)) {  //The node transmitter is a ground station
        longLatStartPosition = cCoordGeo(lutMobility->getLUTPositionY(), lutMobility->getLUTPositionX(), 0);
        longLatEndPosition = cCoordGeo(lutMobility->getLUTPositionY(), lutMobility->getLUTPositionX(), 0);
    } else {  //other
        longLatStartPosition = cCoordGeo(0, 0, 0);
        longLatEndPosition = cCoordGeo(0, 0, 0);
    }
    return new SatelliteUnitDiskTransmission(transmitter, packet, startTime, endTime, preambleDuration, headerDuration, dataDuration,
            startPosition, endPosition, startOrientation, endOrientation, communicationRange, interferenceRange, detectionRange,
            longLatStartPosition, longLatEndPosition);
}

} /* namespace physicallayer */
} /* namespace inet */
