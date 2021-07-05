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

#include "NoradA.h"

#include <ctime>
#include <cmath>
#include <fstream>

#include "libnorad/cTLE.h"
#include "libnorad/cOrbitA.h"
#include "libnorad/cSite.h"

using namespace omnetpp;
Define_Module(NoradA);

// Provides the functionality for satellite positioning
// this class provides the functionality needed to get the positions for satellites according
// to current tables from web information by providing known data
// This class has been adapted from the Norad class so that orbits can be propagated without the
// requirement of a TLE file.

NoradA::NoradA()
{
    gap = 0.0;
    orbit = nullptr;
}

void NoradA::finish()
{
    delete orbit;
}

void NoradA::initializeMobility(const simtime_t& targetTime)
{
    //Instead of reading a TLE file and obtaining the Keplerian elements, the parameters are obtained
    //from the default NED file parameters or the parameters specified within the INI file of the simulation.
    int satIndex = par("satIndex");
    satelliteIndex = satIndex;
    std::string satNameA = par("satName");
    int epochY = par("epochYear");
    double epochD = par("epochDay");
    double mMotion = par("meanMotion");
    double ecc = par("eccentricity");
    double incl = par("inclination");
    double meanAnom = par("meanAnomaly");
    double bstarA = par("bstar");
    double dragA = par("drag");
    planes = par("planes");
    satPerPlane = par("satPerPlane");

    std::string satelliteName = getParentModule()->par("satelliteName").stringValue();

    //The new cOrbitA orbital propagator class is called which passes these Keplerian elements rather than the TLE file.
    orbit = new cOrbitA(satNameA, epochY, epochD, mMotion, ecc, incl, meanAnom, bstarA, dragA, satIndex, planes, satPerPlane);
    raan = orbit->RAAN();
    inclination = orbit->Inclination();

    // Gap is needed to eliminate different start times
    gap = orbit->TPlusEpoch(currentJulian);

    updateTime(targetTime);

}

/**
 * Update the orbit given a target time.
 */
void NoradA::updateTime(const simtime_t& targetTime)
{
    orbit->getPosition((gap + targetTime.dbl()) / 60, &eci);
    geoCoord = eci.toGeo();
}

/**
 * Return longitude of the node.
 */
double NoradA::getLongitude()
{
    return rad2deg(geoCoord.m_Lon);
}

/**
 * Return latitude of the node.
 */
double NoradA::getLatitude()
{
    return rad2deg(geoCoord.m_Lat);
}

/**
 * Get the RAAN of a node. Use to determine the orbital plane of the satellite.
 */
double NoradA::getRaan()
{
    return raan;
}

/**
 * Get the inclination of a node. Use to determine the orbital plane of the satellite.
 */
double NoradA::getInclination()
{
    return inclination;
}

double NoradA::getElevation(const double& refLatitude, const double& refLongitude, const double& refAltitude)
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
double NoradA::getAzimuth(const double& refLatitude, const double& refLongitude, const double& refAltitude)
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
double NoradA::getAltitude()
{
    geoCoord = eci.toGeo();
    return geoCoord.m_Alt;
}

/**
 * Get the straight-line distance between the satellite and coordinates.
 */
double NoradA::getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    double distance = topoLook.m_Range;
    return distance;
}

/**
 * Primary method that is used to check whether, given the index of a satellite, whether it is compatible as an inter-
 * satellite link. It starts by checking whether the satellite is a part of the same plane, and if it is whether or not
 * they are adjacent. It then also checks whether, if they are not part of the same plane, whether the satellites are
 * the same index within neighbouring planes, as this is a valid connection. This method is used within the
 * SatelliteNetworkConfigurator class to filter links that are not compatible.
 */
bool NoradA::isInterSatelliteLink(const int sat2Index)
{
    int currentPlaneSat1 = trunc(satelliteIndex/satPerPlane);
    int currentPlaneSat2 = trunc(sat2Index/satPerPlane);
    if (currentPlaneSat1 == currentPlaneSat2){
        int minSat = ((satPerPlane) * currentPlaneSat1);
        int maxSat = (minSat + (satPerPlane-1));
        if((satelliteIndex+1 == sat2Index) or (satelliteIndex-1 == sat2Index)){
            return true; //Same Plane, adjacent satellite
        }
        else if((satelliteIndex==maxSat && sat2Index==minSat) || (satelliteIndex==minSat && sat2Index==maxSat)){
            return true; //Same Plane, adjacent satellite edge case
        }
    }
    else if ((currentPlaneSat1 == currentPlaneSat2-1) || (currentPlaneSat1 == currentPlaneSat2+1)  //Neighbouring Planes
              || (currentPlaneSat1 == planes && currentPlaneSat2==0)
              || (currentPlaneSat1 == 0 && currentPlaneSat2 == planes)){
        int planeIndexSat1 = (satelliteIndex % (planes*satPerPlane))-(satPerPlane*currentPlaneSat1);
        int planeIndexSat2 = (sat2Index % (planes*satPerPlane))-(satPerPlane*currentPlaneSat2);
        if(planeIndexSat1 == planeIndexSat2){   //Satellites are are adjacent on neighbouring planes
            return true;
        }
    }
    return false;
}

void NoradA::handleMessage(cMessage* msg)
{
    error("Error in Norad::handleMessage(): This module is not able to handle messages.");
}

void NoradA::setJulian(std::tm* currentTime)
{
    currentJulian = cJulian(currentTime->tm_year + 1900,
                            currentTime->tm_mon + 1,
                            currentTime->tm_mday,
                            currentTime->tm_hour,
                            currentTime->tm_min, 0);
}

