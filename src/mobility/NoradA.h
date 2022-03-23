#ifndef LEOSATELLITES_MOBILITY_NORADA_H_
#define LEOSATELLITES_MOBILITY_NORADA_H_

#include <omnetpp.h>

#include <string>
#include <ctime>

#include "libnorad/cEci.h"
#include "libnorad/cJulian.h"
#include "libnorad/ccoord.h"
#include "INorad.h"
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
// Modified by Aiden Valentine, original from the OS3 framework
//-----------------------------------------------------
class NoradA : public INorad
{
public:
    NoradA();
    // Updates the end time of current linear movement for calculation of current position
    // targetTime: End time of current linear movement
    virtual void updateTime(const simtime_t& targetTime);

    virtual void finish();
    // This method gets the current simulation time, cares for the file download (happens only once)
    // of the TLE files from the web and reads the values for the satellites according to the
    // omnet.ini-file. The information is provided by the respective mobility class.
    // targetTime: End time of current linear movement
    virtual void initializeMobility(const simtime_t& targetTime);

    double getRaan();
    double getInclination();

    const int getSatelliteNumber(){return satelliteIndex;};
    const int getNumberOfPlanes(){return planes;}
    const int getSatellitesPerPlane(){return satPerPlane;}
    // Checks if given an index the satellite is a valid inter-satellite link.
    bool isInterSatelliteLink(const int sat2Index);

    bool isReachable(const double& refLatitude, const double& refLongitude, const double& refAltitude = -9999);
private:

    int satelliteIndex;
    int planes;
    int satPerPlane;
    double elevationAngle;

    cOrbitA* orbit;
};

#endif
