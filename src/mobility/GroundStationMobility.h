#ifndef LEOSATELLITES_MOBILITY_GROUNDSTATIONMOBILITY_H_
#define LEOSATELLITES_MOBILITY_GROUNDSTATIONMOBILITY_H_

#include <mobility/LUTMotionMobility.h>    // os3

class GroundStationMobility : public LUTMotionMobility{
    public:
        // returns the Euclidean distance from ground station to reference point - Implemented by Aiden Valentine
        virtual double getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999) const;

};

#endif /* LEOSATELLITES_MOBILITY_GROUNDSTATIONMOBILITY_H_ */
