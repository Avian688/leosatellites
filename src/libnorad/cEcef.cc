#include "cEcef.h"
#include "libnorad/globals.h"
#include "libnorad/ccoord.h"
#include <math.h>
//////////////////////////////////////////////////////////////////////
// cEcef(cCoordGeo&)
// Calculate the ECEF coordinates of a location, for simple distance calculations.
// Measurement is in meters unlike OS3 being in kilometers, therefore the altitude is translated.
// Reference: https://www2.unb.ca/gge/Pubs/LN39.pdf (GEODETIC POSITIONS COMPUTATIONS by E. J. KRAKIWSKY and D. B. THOMSON)
// Written by Aiden Valentine
cEcef::cEcef() {
    x = 0;
    y = 0;
    z = 0;
}

/**
 * Constructor which, upon being given longitude,latitude and altitude coordinates, calculates the ECEF coordinates.
 */
cEcef::cEcef(cCoordGeo geoCoordinates) {
    const double e2 = 6.6943799901377997e-3;
    double phi = geoCoordinates.m_Lat*0.0174533; //degrees to radians
    double lambda = geoCoordinates.m_Lon*0.0174533;
    double h = geoCoordinates.m_Alt*1000; //km to m

    double NPhi = SEMI_MAJOR_AXIS / sqrt(1-e2*sqr(sin(phi)));

    x = (NPhi+h)*cos(phi)*cos(lambda);
    y = (NPhi+h)*cos(phi)*sin(lambda);
    z = (NPhi*(1-e2)+h)*sin(phi);
}

cEcef::cEcef(double latitude, double longitude, double altitude): cEcef(cCoordGeo(latitude, longitude, altitude)) {}

/**
 * Get Distances between two ECEF coordinates using Pythagoras' Theorem
 */
double cEcef::getDistance(cEcef receiverEcef) {
    double x2 = receiverEcef.getX();
    double y2 = receiverEcef.getY();
    double z2 = receiverEcef.getZ();
    return sqrt(sqr(x2-x) + sqr(y2-y) + sqr(z2-z));
}
