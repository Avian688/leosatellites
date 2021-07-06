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

#ifndef LEOSATELLITES_MOBILITY_INORAD_H_
#define LEOSATELLITES_MOBILITY_INORAD_H_

#include <omnetpp.h>

#include <string>
#include <ctime>

#include "libnorad/cEci.h"
#include "libnorad/cJulian.h"
#include "libnorad/ccoord.h"

using namespace omnetpp;

//-----------------------------------------------------
// Class: INorad
//
// Provides the functionality for satellite positioning
// this class provides the functionality needed to get the positions for satellites according
// to current tables from web information by providing known data
// This class has been adapted from the Norad class so that orbits can be propagated without the
// requirement of a TLE file.
// Written by Aiden Valentine
//-----------------------------------------------------
class INorad : public cSimpleModule
{
public:

    virtual cJulian getJulian(){return currentJulian;};
    // sets the internal calendar by translating the current gregorian time
    // currentTime: time at which the simulation takes place
    virtual void setJulian(std::tm* currentTime);

    virtual void updateTime(const simtime_t& targetTime){};

    // This method gets the current simulation time, cares for the file download (happens only once)
    // of the TLE files from the web and reads the values for the satellites according to the
    // omnet.ini-file. The information is provided by the respective mobility class.
    // targetTime: End time of current linear movement
    virtual void initializeMobility(const simtime_t& targetTime){};

    // returns the longitude
    virtual double getLongitude();

    // returns the latitude
    virtual double getLatitude();

    // returns the elevation to a reference point
    virtual double getElevation(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);

    // returns the azimuth
    virtual double getAzimuth(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);

    // returns the altitude
    virtual double getAltitude();


    // returns the distance to the satellite from a reference point (distance in km)
    virtual double getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);

protected:
    virtual void handleMessage(cMessage* msg);

protected:
    cEci eci;
    cJulian currentJulian;

    double gap;
    cCoordGeo geoCoord;
};

#endif
