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

#include "INorad.h"

#include <ctime>
#include <cmath>
#include <fstream>

#include "libnorad/cTLE.h"
#include "../libnorad/cOrbitA.h"
#include "libnorad/cSite.h"

using namespace omnetpp;
Register_Abstract_Class(INorad);
//Define_Module(INorad);

// Provides the functionality for satellite positioning
// this class provides the functionality needed to get the positions for satellites according
// to current tables from web information by providing known data
// This class has been adapted from the Norad class so that orbits can be propagated without the
// requirement of a TLE file.

/**
 * Return longitude of the node.
 */
double INorad::getLongitude()
{
    return rad2deg(geoCoord.m_Lon);
}

/**
 * Return latitude of the node.
 */
double INorad::getLatitude()
{
    return rad2deg(geoCoord.m_Lat);
}

double INorad::getElevation(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    if (topoLook.m_El == 0.0) {
        error("Error in Norad::getElevation(): Corrupted database.");
    }
    return rad2deg(topoLook.m_El);
}

/**
 * Get the azimuth or the satellite. This is an angular measurement used in spherical coordinate systems.
 */
double INorad::getAzimuth(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    if (topoLook.m_El == 0.0) {
        error("Error in Norad::getAzimuth(): Corrupted database.");
    }
    return rad2deg(topoLook.m_Az);
}

/**
 * Get the altitude of a node.
 */
double INorad::getAltitude()
{
    geoCoord = eci.toGeo();
    return geoCoord.m_Alt;
}

/**
 * Get the straight-line distance between the satellite and coordinates.
 */
double INorad::getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    double distance = topoLook.m_Range;
    return distance;
}


void INorad::handleMessage(cMessage* msg)
{
    error("Error in Norad::handleMessage(): This module is not able to handle messages.");
}

void INorad::setJulian(std::tm* currentTime)
{
    currentJulian = cJulian(currentTime->tm_year + 1900,
                            currentTime->tm_mon + 1,
                            currentTime->tm_mday,
                            currentTime->tm_hour,
                            currentTime->tm_min, 0);
}

