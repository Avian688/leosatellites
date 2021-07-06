#include "NoradTLE.h"

#include <ctime>
#include <fstream>

#include "libnorad/cTLE.h"
#include "libnorad/cOrbit.h"
#include "libnorad/cSite.h"

using namespace omnetpp;
Define_Module(NoradTLE);

NoradTLE::NoradTLE()
{
    gap = 0.0;
    tle = nullptr;
    orbit = nullptr;
}

void NoradTLE::finish()
{
    delete orbit;
    delete tle;
}

void NoradTLE::initializeMobility(const simtime_t& targetTime)
{
    std::string filename = par("TLEfile").stringValue();

    // read file with TLE data
    std::fstream tleFile;
    tleFile.open(filename.c_str());

    // Length 100 should be enough since lines are usually 70+'\n' char long
    char line[100]     = "";
    char line1tmp[100] = "";
    char line2tmp[100] = "";

    std::string satelliteName = getParentModule()->par("satelliteName").stringValue();
    std::string line_str;
    if (satelliteName == "") {
        int index = getParentModule()->getIndex();
        int i = 0;
        do {
            tleFile.getline(line, 100);
            if (!tleFile.good()) {
                EV << "Error in Norad::initializeMobility(): Cannot read further satellites from TLE file!" << std::endl;
                endSimulation();
            }
        } while (i++ < index * 3 && tleFile.good());
        line_str.append(line);
    } else {
        do {
            line_str = "";
            tleFile.getline(line, 100);
            line_str.append(line);
        } while (tleFile.good()
                && line_str.find(satelliteName.c_str()) == std::string::npos);
    }
    tleFile.getline(line1tmp, 100);
    tleFile.getline(line2tmp, 100);

    // Pretty up the satellites name
    line_str = line_str.substr(0, line_str.find("  "));
    line0 = line_str;
    line1.append(line1tmp);
    line2.append(line2tmp);
    cTle tle(line0, line1, line2);
    orbit = new cOrbit(tle);

    raan = orbit->RAAN();
    inclination = orbit->Inclination();

    // Gap is needed to eliminate different start times
    gap = orbit->TPlusEpoch(currentJulian);

    updateTime(targetTime);

    // Set name from TLE file for icon name
    line3 = orbit->SatName(false);
}

void NoradTLE::updateTime(const simtime_t& targetTime)
{
    orbit->getPosition((gap + targetTime.dbl()) / 60, &eci);
    geoCoord = eci.toGeo();
}

double NoradTLE::getLongitude()
{
    return rad2deg(geoCoord.m_Lon);
}

double NoradTLE::getLatitude()
{
    return rad2deg(geoCoord.m_Lat);
}

double NoradTLE::getRaan()
{
    return raan;
}

double NoradTLE::getInclination()
{
    return inclination;
}

double NoradTLE::getElevation(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    if (topoLook.m_El == 0.0) {
        error("Error in Norad::getElevation(): Corrupted database.");
    }
    return rad2deg(topoLook.m_El);
}

double NoradTLE::getAzimuth(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    if (topoLook.m_El == 0.0) {
        error("Error in Norad::getAzimuth(): Corrupted database.");
    }
    return rad2deg(topoLook.m_Az);
}

double NoradTLE::getAltitude()
{
    geoCoord = eci.toGeo();
    return geoCoord.m_Alt;
}

double NoradTLE::getDistance(const double& refLatitude, const double& refLongitude, const double& refAltitude)
{
    cSite siteEquator(refLatitude, refLongitude, refAltitude);
    cCoordTopo topoLook = siteEquator.getLookAngle(eci);
    double distance = topoLook.m_Range;
    return distance;
}

void NoradTLE::handleMessage(cMessage* msg)
{
    error("Error in Norad::handleMessage(): This module is not able to handle messages.");
}

void NoradTLE::setJulian(std::tm* currentTime)
{
    currentJulian = cJulian(currentTime->tm_year + 1900,
                            currentTime->tm_mon + 1,
                            currentTime->tm_mday,
                            currentTime->tm_hour,
                            currentTime->tm_min, 0);
}

