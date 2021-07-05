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

#ifndef __OS3_NoradA_H__
#define __OS3_NoradA_H__

#include <omnetpp.h>

#include <string>
#include <ctime>

#include "libnorad/cEci.h"
#include "libnorad/cJulian.h"
#include "libnorad/ccoord.h"

using namespace omnetpp;
class cOrbitA;

//-----------------------------------------------------
// Class: NoradA
//
// Provides the functionality for satellite positioning
// this class provides the functionality needed to get the positions for satellites according
// to current tables from web information by providing known data
// This class has been adapted from the Norad class so that orbits can be propagated without the
// requirement of a TLE file.
// Written by Aiden Valentine
//-----------------------------------------------------
class NoradA : public cSimpleModule
{
public:
    NoradA();

    // sets the internal calendar by translating the current gregorian time
    // currentTime: time at which the simulation takes place
    void setJulian(std::tm* currentTime);

    // Updates the end time of current linear movement for calculation of current position
    // targetTime: End time of current linear movement
    void updateTime(const simtime_t& targetTime);

    // This method gets the current simulation time, cares for the file download (happens only once)
    // of the TLE files from the web and reads the values for the satellites according to the
    // omnet.ini-file. The information is provided by the respective mobility class.
    // targetTime: End time of current linear movement
    virtual void initializeMobility(const simtime_t& targetTime);

    // returns the longitude
    double getLongitude();

    // returns the latitude
    double getLatitude();

    double getRaan();
    double getInclination();

    // returns the elevation to a reference point
    double getElevation(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);

    // returns the azimuth
    double getAzimuth(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);

    // returns the altitude
    double getAltitude();

    void finish();

    const int getSatelliteNumber(){return satelliteIndex;};
    const int getNumberOfPlanes(){return planes;}
    const int getSatellitesPerPlane(){return satPerPlane;}
    // Checks if given an index the satellite is a valid inter-satellite link.
    bool isInterSatelliteLink(const int sat2Index);

    // returns the distance to the satellite from a reference point (distance in km)
    double getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);

protected:
    virtual void handleMessage(cMessage* msg);

private:
    cEci eci;
    cJulian currentJulian;

    int satelliteIndex;
    int planes;
    int satPerPlane;
    double inclination; //inclination and raan used to determine orbital plane
    double raan;

    //double argPerigee;

    double gap;
    cOrbitA* orbit;
    cCoordGeo geoCoord;
};

#endif
