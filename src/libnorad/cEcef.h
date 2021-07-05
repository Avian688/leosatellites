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
