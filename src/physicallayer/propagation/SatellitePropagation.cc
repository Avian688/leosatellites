//
// Copyright (C) 2013 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "SatellitePropagation.h"
#include "../unitdisk/SatelliteUnitDiskTransmission.h"
#include "../apskradio/SatelliteApskScalarTransmission.h"
#include "mobility/SatSGP4Mobility.h"
#include "mobility/LUTMotionMobility.h"
#include "../../mobility/GroundStationMobility.h"
#include "../../mobility/SatelliteMobility.h"
#include "libnorad/ccoord.h"
#include "libnorad/cEci.h"
#include "../../libnorad/cEcef.h"
#include "inet/physicallayer/common/packetlevel/Arrival.h"
#include <math.h>
namespace inet {

namespace physicallayer {

Define_Module(SatellitePropagation);

/**
 * Contructor for the SatellitePropagation model. It must also call the constructor for the
 * PropagationBase to be compatible with the radio medium models of INET.
 */
SatellitePropagation::SatellitePropagation() :
    PropagationBase(),
    ignoreMovementDuringTransmission(false),
    ignoreMovementDuringPropagation(false),
    ignoreMovementDuringReception(false)
{
}

/**
 *Intialise method of SatellitePropagation model.This method simply initialises specific parameters
 *that enable further customisation of the model. The same implementation is used within the
 *INET ConstantSpeedPropagation, which this model is based off.
 */
void SatellitePropagation::initialize(int stage)
{
    PropagationBase::initialize(stage);
    if (stage == INITSTAGE_LOCAL) {
        ignoreMovementDuringTransmission = par("ignoreMovementDuringTransmission");
        ignoreMovementDuringPropagation = par("ignoreMovementDuringPropagation");
        ignoreMovementDuringReception = par("ignoreMovementDuringReception");
        // TODO:
        if (!ignoreMovementDuringTransmission)
            throw cRuntimeError("ignoreMovementDuringTransmission is yet not implemented");
    }
}

/**
 * This method has no functionality but is only used due to its being a part of the INET ConstantSpeedPropagation model.
 * INET has not implemented this method yet, and as a result, it does not affect the model. The method was implemnted so
 * that when INET does adapt the propagation models of INET, the SatellitePropagation model should be robust enough to
 * easily adapt.
 */
const Coord SatellitePropagation::computeArrivalPosition(const simtime_t time, const Coord position, IMobility *mobility) const
{
    // TODO: return mobility->getPosition(time);
    throw cRuntimeError("Movement approximation is not implemented");
}

std::ostream& SatellitePropagation::printToStream(std::ostream& stream, int level) const
{
    stream << "SatellitePropagation";
    if (level <= PRINT_LEVEL_TRACE)
        stream << ", ignoreMovementDuringTransmission = " << ignoreMovementDuringTransmission
               << ", ignoreMovementDuringPropagation = " << ignoreMovementDuringPropagation
               << ", ignoreMovementDuringReception = " << ignoreMovementDuringReception;
    return PropagationBase::printToStream(stream, level);
}

/**
 * computeArrival is the primary method of the SatellitePropagationModel. Whenever a transmission has been made in INET,
 * the propagation model is used to compute the arrival, which returns an Arrival object with the start and end propagation
 * times. The ConstantSpeedPropagation model returns a propagation delay equal to the distance between two nodes on the OMNeT++
 * 2D canvas. This method adapts this by firstly checking if the transmission model is the custom SatelliteUnitDiskTransmission.
 * This transmission model encapsulates the coordinates so that the corresponding receiver nodes getDistance method can use the
 * coordinates and get the actual distance.
 */
const IArrival *SatellitePropagation::computeArrival(const ITransmission *transmission, IMobility *mobility) const
{
    arrivalComputationCount++;
    const simtime_t startTime = transmission->getStartTime();
    const simtime_t endTime = transmission->getEndTime();

    const Coord startPosition = transmission->getStartPosition();   //antenna position when the transmitter has started the transmission
    const Coord endPosition = transmission->getEndPosition();

    cEcef transmissionEcef = cEcef();
    cEcef receiverEcef = cEcef();
    double distance = 0; //m
    if (const SatelliteUnitDiskTransmission *satUnitTransmission = dynamic_cast<const SatelliteUnitDiskTransmission*>(transmission)) {
            if (const SatelliteMobility *receiverSatMobility = dynamic_cast<const SatelliteMobility*>(mobility)) {
                distance = receiverSatMobility->getDistance(satUnitTransmission->getStartLongLatPosition().m_Lat,
                           satUnitTransmission->getStartLongLatPosition().m_Lon, satUnitTransmission->getStartLongLatPosition().m_Alt)*1000; //OS3 uses KM for all measurements, which means the altitude in m must be converted.

            } else if (const GroundStationMobility *receiverLutMobility = dynamic_cast<const GroundStationMobility*>(mobility))
            {
                distance = receiverLutMobility->getDistance(satUnitTransmission->getStartLongLatPosition().m_Lat,
                           satUnitTransmission->getStartLongLatPosition().m_Lon, satUnitTransmission->getStartLongLatPosition().m_Alt);
            }

        //APSK - not used in current simulation tests
        } else if (const SatelliteApskScalarTransmission *satApskTransmission = dynamic_cast<const SatelliteApskScalarTransmission*>(transmission)) {
            if (const SatelliteMobility *receiverSatMobility = dynamic_cast<const SatelliteMobility*>(mobility)) {
                distance = receiverSatMobility->getDistance(satApskTransmission->getStartLongLatPosition().m_Lat, satApskTransmission->getStartLongLatPosition().m_Lon, satApskTransmission->getStartLongLatPosition().m_Alt)*1000;
            } else if (const GroundStationMobility *receiverLutMobility = dynamic_cast<const GroundStationMobility*>(mobility))
            {
                distance = receiverLutMobility->getDistance(satApskTransmission->getStartLongLatPosition().m_Lat, satApskTransmission->getStartLongLatPosition().m_Lon, satApskTransmission->getStartLongLatPosition().m_Alt);
            }
            if(distance/1000<=1123){
                EV << "\nDISTANCE: " << distance/1000;
            }
        }else {
            EV << "\nOTHER TRANSMITTER DETECTED";
    }

    const Coord startArrivalPosition = ignoreMovementDuringPropagation ? mobility->getCurrentPosition() : computeArrivalPosition(startTime, startPosition, mobility);
    //const simtime_t startPropagationTime = startPosition.distance(startArrivalPosition) / propagationSpeed.get();
    const simtime_t startPropagationTime =  distance / propagationSpeed.get();
    const simtime_t startArrivalTime = startTime + startPropagationTime;
    const Quaternion startArrivalOrientation = mobility->getCurrentAngularPosition();
    if (ignoreMovementDuringReception) {
        const Coord endArrivalPosition = startArrivalPosition;
        const simtime_t endPropagationTime = startPropagationTime;
        const simtime_t endArrivalTime = endTime + startPropagationTime;
        const simtime_t preambleDuration = transmission->getPreambleDuration();
        const simtime_t headerDuration = transmission->getHeaderDuration();
        const simtime_t dataDuration = transmission->getDataDuration();
        const Quaternion endArrivalOrientation = mobility->getCurrentAngularPosition();
        return new Arrival(startPropagationTime, endPropagationTime, startArrivalTime, endArrivalTime, preambleDuration, headerDuration, dataDuration, startArrivalPosition, endArrivalPosition, startArrivalOrientation, endArrivalOrientation);
    }
    else {
        const Coord endArrivalPosition = computeArrivalPosition(endTime, endPosition, mobility);
        const simtime_t endPropagationTime = endPosition.distance(endArrivalPosition) / propagationSpeed.get();
        const simtime_t endArrivalTime = endTime + endPropagationTime;
        const simtime_t preambleDuration = transmission->getPreambleDuration();
        const simtime_t headerDuration = transmission->getHeaderDuration();
        const simtime_t dataDuration = transmission->getDataDuration();
        const Quaternion endArrivalOrientation = mobility->getCurrentAngularPosition();
        return new Arrival(startPropagationTime, endPropagationTime, startArrivalTime, endArrivalTime, preambleDuration, headerDuration, dataDuration, startArrivalPosition, endArrivalPosition, startArrivalOrientation, endArrivalOrientation);
    }
}

} // namespace physicallayer

} // namespace inet

