#ifndef OS3_LIBNORAD_CECEF_H_
#define OS3_LIBNORAD_CECEF_H_

#include "libnorad/ccoord.h"

//-----------------------------------------------------
// Class: CEcef
//
// The Earth-Centered, Earth-Fixed Coordinate system translates a given nodes longitude, latitude and altitude
// data to x, y and z coordinates relative to the origin (the centre of the earth). This coordinate system was
// used as the cEcef coordinate system requires the current Julian time of a given node, which is not stored
// within the LUTMotionMobility class. This could have been implemented but it would require constant updating,
// potentially affecting the performance of the simulation depending on the amount of ground stations used.
// Written by Aiden Valentine
// References: https://uk.mathworks.com/help/aeroblks/llatoecefposition.html
//-----------------------------------------------------
class cEcef {
    public:
        cEcef();
        cEcef(cCoordGeo geoCoordinates);
        cEcef(double latitude, double longitude, double altitude);
        double getDistance(cEcef receiverEcef);
        double getX(){return x;};
        double getY(){return y;};
        double getZ(){return z;};
        virtual ~cEcef(){};
    private:
        double x;
        double y;
        double z;
};

#endif /* OS3_LIBNORAD_CECEF_H_ */
