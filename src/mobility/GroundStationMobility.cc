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

#include "GroundStationMobility.h"
#include "../libnorad/cEcef.h"
Define_Module(GroundStationMobility);

double GroundStationMobility::getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude) const
{
    cEcef ecefSourceCoord = cEcef(getLUTPositionY(),getLUTPositionX(),0); //could change altitude to real value
    cEcef ecefDestCoord = cEcef(refLatitude,refLongitude,refAltitude); //could change altitude to real value
    return ecefSourceCoord.getDistance(ecefDestCoord);
}

