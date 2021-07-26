#include "SatelliteMobility.h"
#include "libnorad/cJulian.h"

#include <ctime>
#include <cmath>

#include "INorad.h"
#include "NoradA.h"

Define_Module(SatelliteMobility);

SatelliteMobility::SatelliteMobility()
{
   noradModule = nullptr;
   mapX = 0;
   mapY = 0;
   transmitPower = 0.0;
}

void SatelliteMobility::initialize(int stage)
{
    // noradModule must be initialized before LineSegmentsMobilityBase calling setTargetPosition() in its initialization at stage 1
    if (stage == 1) {
        noradModule->initializeMobility(nextChange);
    }
    LineSegmentsMobilityBase::initialize(stage);

    noradModule = check_and_cast< INorad* >(getParentModule()->getSubmodule("NoradModule"));
    if (noradModule == nullptr) {
        error("Error in SatSGP4Mobility::initializeMobility(): Cannot find module Norad.");
    }

    //std::time_t timestamp = std::time(nullptr);       // get current time as an integral value holding the num of secs
    std::time_t timestamp =  1619119189;  //8:20PM 22/04/2021                                             // since 00:00, Jan 1 1970 UTC
    std::tm* currentTime = std::gmtime(&timestamp);   // convert timestamp into structure holding a calendar date and time
    noradModule->INorad::setJulian(currentTime);

    mapX = std::atoi(getParentModule()->getParentModule()->getDisplayString().getTagArg("bgb", 0));
    mapY = std::atoi(getParentModule()->getParentModule()->getDisplayString().getTagArg("bgb", 1));

    EV << "initializing SatSGP4Mobility stage " << stage << endl;
    WATCH(lastPosition);

    //move(); //updateVisualRepresentation();
}

bool SatelliteMobility::isOnSameOrbitalPlane(double raan2, double inclination2)
{
    if(NoradA *noradAModule = dynamic_cast<NoradA*>(noradModule)){
        double raan = noradAModule->getRaan();
        double inclination = noradAModule->getInclination();
        if((inclination == inclination2) && (raan == raan2))
        {
            return true;
        }
    }
    return false;
}

double SatelliteMobility::getAltitude() const
{
    return noradModule->getAltitude();
}

double SatelliteMobility::getElevation(const double& refLatitude, const double& refLongitude,
                                     const double& refAltitude) const
{
    return noradModule->getElevation(refLatitude, refLongitude, refAltitude);
}

double SatelliteMobility::getAzimuth(const double& refLatitude, const double& refLongitude,
                                   const double& refAltitude) const
{
    return noradModule->getAzimuth(refLatitude, refLongitude, refAltitude);
}

double SatelliteMobility::getDistance(const double& refLatitude, const double& refLongitude,
                                    const double& refAltitude) const
{
    return noradModule->getDistance(refLatitude, refLongitude, refAltitude);
}

double SatelliteMobility::getLongitude() const
{
    return noradModule->getLongitude();
}

double SatelliteMobility::getLatitude() const
{
    return noradModule->getLatitude();
}

void SatelliteMobility::setTargetPosition()
{
    nextChange += updateInterval.dbl();
    noradModule->updateTime(nextChange);
    lastPosition.x = mapX * noradModule->getLongitude() / 360 + (mapX / 2);
    lastPosition.x = static_cast<int>(lastPosition.x) % static_cast<int>(mapX);
    lastPosition.y = ((-mapY * noradModule->getLatitude()) / 180) + (mapY / 2);
    targetPosition.x = lastPosition.x;
    targetPosition.y = lastPosition.y;
}

void SatelliteMobility::move()
{
    LineSegmentsMobilityBase::move();
    raiseErrorIfOutside();
}

void SatelliteMobility::fixIfHostGetsOutside()
{
    raiseErrorIfOutside();
}

