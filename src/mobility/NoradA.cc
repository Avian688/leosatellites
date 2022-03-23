#include "NoradA.h"

#include <ctime>
#include <cmath>
#include <fstream>

#include "libnorad/cTLE.h"
#include "../libnorad/cOrbitA.h"
#include "libnorad/cSite.h"

using namespace omnetpp;
Define_Module(NoradA);

// Provides the functionality for satellite positioning
// this class provides the functionality needed to get the positions for satellites according
// to current tables from web information by providing known data
// This class has been adapted from the Norad class so that orbits can be propagated without the
// requirement of a TLE file. - Aiden Valentine

NoradA::NoradA()
{
    gap = 0.0;
    orbit = nullptr;
    planes = 0;
    satPerPlane = 0;
}

void NoradA::finish()
{
    delete orbit;
}

/**
 * Update the orbit given a target time.
 */
void NoradA::updateTime(const simtime_t& targetTime)
{
    orbit->getPosition((gap + targetTime.dbl()) / 60, &eci);
    geoCoord = eci.toGeo();
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
    //double mMotion = par("meanMotion");
    double ecc = par("eccentricity");
    double incl = par("inclination");
    double meanAnom = par("meanAnomaly");
    double bstarA = par("bstar");
    double dragA = par("drag");
    int phaseOffset = par("phaseOffset");
    planes = par("planes");
    satPerPlane = par("satPerPlane");
    elevationAngle = par("elevationAngle");
    double altitude = par("altitude");

    std::string satelliteName = getParentModule()->par("satelliteName").stringValue();

    //The new cOrbitA orbital propagator class is called which passes these Keplerian elements rather than the TLE file.
    orbit = new cOrbitA(satNameA, epochY, epochD, altitude, ecc, incl, meanAnom, bstarA, dragA, phaseOffset, satIndex, planes, satPerPlane);

    // Gap is needed to eliminate different start times
    gap = orbit->TPlusEpoch(currentJulian);
    updateTime(targetTime);
}

/**
 * Get the RAAN of a node. Use to determine the orbital plane of the satellite.
 */
double NoradA::getRaan()
{
    return orbit->RAAN();
}

/**
 * Get the inclination of a node. Use to determine the orbital plane of the satellite.
 */
double NoradA::getInclination()
{
    return orbit->Inclination();
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

bool NoradA::isReachable(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    return getElevation(refLatitude, refLongitude, refAltitude) > elevationAngle;
}

