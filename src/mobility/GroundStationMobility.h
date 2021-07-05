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

#ifndef LEOSATELLITES_MOBILITY_GROUNDSTATIONMOBILITY_H_
#define LEOSATELLITES_MOBILITY_GROUNDSTATIONMOBILITY_H_

#include <mobility/LUTMotionMobility.h>    // os3

class GroundStationMobility : public LUTMotionMobility{
    public:
        // returns the Euclidean distance from ground station to reference point - Implemented by Aiden Valentine
        virtual double getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999) const;

};

#endif /* LEOSATELLITES_MOBILITY_GROUNDSTATIONMOBILITY_H_ */
