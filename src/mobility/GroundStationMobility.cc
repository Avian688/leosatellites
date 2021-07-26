#include "GroundStationMobility.h"
#include "../libnorad/cEcef.h"
Define_Module(GroundStationMobility);

double GroundStationMobility::getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude) const
{
    cEcef ecefSourceCoord = cEcef(getLUTPositionY(),getLUTPositionX(),0); //could change altitude to real value
    cEcef ecefDestCoord = cEcef(refLatitude,refLongitude,refAltitude); //could change altitude to real value
    return ecefSourceCoord.getDistance(ecefDestCoord);
}

