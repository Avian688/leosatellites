//-----------------------------------------------------
// cOrbitA.cpp
//
// Copyright (c) 2002-2003 Michael F. Henry
// Updated for use with models implemented by Aiden Valentine
// mfh 11/15/2003
//-----------------------------------------------------

#include "cOrbitA.h"

#include <cmath>
#include <ctime>
#include <cassert>

#include "libnorad/cVector.h"
#include "libnorad/cEci.h"
#include "libnorad/ccoord.h"
#include "libnorad/cJulian.h"
#include "libnorad/cNoradSGP4.h"
#include "libnorad/cNoradSDP4.h"
#include "cNoradSGP4A.h"

// This class accepts a single satellite's Keplerian elements and provides information
// regarding the satellite's orbit such as period, axis length,
// ECI coordinates/velocity, etc., using the SGP4/SDP4 orbital models.
cOrbitA::cOrbitA(std::string satNameA, int epochY, double epochD, double altitude, double ecc, double incl, double meanAnom, double bstarA, double dragA, int phaseOffset, int satIndex, int planes, int satPerPlane) :
   m_pNoradModel(NULL)
{
   satName = satNameA;
   epochYear = epochY;
   epochDay = epochD;
   eccentricity = ecc;
   inclination = incl;
   meanAnomaly = meanAnom;
   argPerigee = 0; //*RADS_PER_DEG;
   bstar = bstarA;
   drag = dragA;
   //int offsetVal = planes; //offsetVal must be changed according to constellation, used to prevent collisions
   int currentPlane = trunc(satIndex/satPerPlane);
   int planeIndex = (satIndex % (planes*satPerPlane))-(satPerPlane*currentPlane); //index of a satellite within a plane
   raan = ((360.0/planes)*currentPlane) * RADS_PER_DEG; //RAAN value, uniformly created so that there are equally spaced orbital planes for even coverage.
   double phaseOffsetVal = ((360.0/satPerPlane)*(phaseOffset/planes))*currentPlane;
   meanAnomaly = (((360.0/satPerPlane)*planeIndex))*RADS_PER_DEG; //Denotes the position of a satellite within its plane.
   if (epochYear < 57)
      epochYear += 2000;
   else
      epochYear += 1900;

   m_jdEpoch = cJulian(epochYear, epochDay);

   m_secPeriod = -1.0;

   double velocity = sqrt(GM/(XKMPER_WGS72+altitude));
   meanMotion = SEC_PER_DAY/((TWOPI*(XKMPER_WGS72+altitude))/velocity);
   // Recover the original mean motion and semimajor axis from the
   // input elements.
   const double mm     = meanMotion;
   const double rpmin  = mm * 2 * PI / MIN_PER_DAY;   // rads per minute

   const double a1     = std::pow(XKE / rpmin, TWOTHRD);
   const double e      = eccentricity;
   const double i      = inclination;
   const double temp   = (1.5 * CK2 * (3.0 * sqr(cos(i)) - 1.0) / std::pow(1.0 - e * e, 1.5));
   const double delta1 = temp / (a1 * a1);
   const double a0     = a1 *
                      (1.0 - delta1 *
                      ((1.0 / 3.0) + delta1 *
                      (1.0 + 134.0 / 81.0 * delta1)));

   const double delta0 = temp / (a0 * a0);

   m_mnMotionRec        = rpmin / (1.0 + delta0);
   m_aeAxisSemiMinorRec = a0 / (1.0 - delta0);
   m_aeAxisSemiMajorRec = m_aeAxisSemiMinorRec / sqrt(1.0 - (e * e));
   m_kmPerigeeRec       = XKMPER_WGS72 * (m_aeAxisSemiMajorRec * (1.0 - e) - AE);
   m_kmApogeeRec        = XKMPER_WGS72 * (m_aeAxisSemiMajorRec * (1.0 + e) - AE);

   m_pNoradModel = new cNoradSGP4A(*this);
}

cOrbitA::~cOrbitA()
{
   delete m_pNoradModel;
}

//-----------------------------------------------------
// Return the period in seconds
//-----------------------------------------------------
double cOrbitA::Period() const
{
   if (m_secPeriod < 0.0) {
      // Calculate the period using the recovered mean motion.
      if (m_mnMotionRec == 0)
         m_secPeriod = 0.0;
      else
         m_secPeriod = (2 * PI) / m_mnMotionRec * 60.0;
   }
   return m_secPeriod;
}

//-----------------------------------------------------
// Returns elapsed number of seconds from epoch to given time.
// Note: "Predicted" TLEs can have epochs in the future.
//-----------------------------------------------------
double cOrbitA::TPlusEpoch(const cJulian& gmt) const
{
   return gmt.spanSec(Epoch());
}

//-----------------------------------------------------
// Returns the mean anomaly in radians at given GMT.
// At epoch, the mean anomaly is given by the elements data.
//-----------------------------------------------------
double cOrbitA::mnAnomaly(cJulian gmt) const
{
   const double span = TPlusEpoch(gmt);
   const double P    = Period();

   assert(P != 0.0);

   return std::fmod(meanAnomaly + (TWOPI * (span / P)), TWOPI);
}

//-----------------------------------------------------
// getPosition()
// This procedure returns the ECI position and velocity for the satellite
// at "tsince" minutes from the (GMT) TLE epoch. The vectors returned in
// the ECI object are kilometer-based.
// tsince  - Time in minutes since the TLE epoch (GMT).
//-----------------------------------------------------
bool cOrbitA::getPosition(double tsince, cEci* pEci) const
{
   const bool rc = m_pNoradModel->getPosition(tsince, *pEci);
   pEci->ae2km();
   return rc;
}

//-----------------------------------------------------
// SatName()
// Return the name of the satellite. If requested, the NORAD number is
// appended to the end of the name, i.e., "ISS (ZARYA) #25544".
// The name of the satellite with the NORAD number appended is important
// because many satellites, especially debris, have the same name and
// would otherwise appear to be the same satellite in ouput data.
//-----------------------------------------------------
std::string cOrbitA::SatName(bool fAppendId /* = false */) const
{
   return satName;
}

