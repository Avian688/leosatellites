//-----------------------------------------------------
// cOrbitA.h
//
// This is the header file for the class cOrbitA. This class accepts a
// single satellite's Keplerian elements and provides information
// regarding the satellite's orbit such as period, axis length,
// ECI coordinates/velocity, etc., using the SGP4/SDP4 orbital models.
//
// Copyright (c) 2002-2003 Michael F. Henry - Adapted by Aiden Valentine
//-----------------------------------------------------

#ifndef LEOSATELLITES_MOBILITY_cOrbitA_H__
#define LEOSATELLITES_MOBILITY_cOrbitA_H__

#include "libnorad/cTLE.h"
#include "libnorad/cJulian.h"

class cVector;
class cGeoCoord;
class cEci;
class cNoradBaseA;

class cOrbitA
{
public:
   cOrbitA(std::string satNameA, int epochY, double epochD, double mMotion, double ecc, double incl, double meanAnom, double bstarA, double dragA, int satIndex, int planes, int satPerPlane);
   virtual ~cOrbitA();

   // Return satellite ECI data at given minutes since element's epoch.
   bool getPosition(double tsince, cEci* pEci) const;

   double Inclination()  const { return inclination;                 }
   double Eccentricity() const { return eccentricity;         }
   double RAAN()         const { return raan;              }
   double ArgPerigee()   const { return argPerigee;            }
   double BStar()        const { return bstar / AE;}
   double Drag()         const { return drag; }
   double mnMotion()     const { return meanMotion;   }
   double mnAnomaly()    const { return meanAnomaly;                 }
   double mnAnomaly(cJulian t) const;  // mean anomaly (in radians) at time t

   cJulian Epoch() const { return m_jdEpoch; }

   double TPlusEpoch(const cJulian& t) const;    // time span [t - epoch] in secs

   std::string SatName(bool fAppendId = false) const;

   // "Recovered" from the input elements
   double SemiMajor()   const { return m_aeAxisSemiMajorRec; }
   double SemiMinor()   const { return m_aeAxisSemiMinorRec; }
   double mnMotionRec() const { return m_mnMotionRec; }  // mn motion, rads/min
   double Major()   const { return 2.0 * SemiMajor(); }  // major axis in AE
   double Minor()   const { return 2.0 * SemiMinor(); }  // minor axis in AE
   double Perigee() const { return m_kmPerigeeRec;    }  // perigee in km
   double Apogee()  const { return m_kmApogeeRec;     }  // apogee in km
   double Period()  const;                               // period in seconds

private:
   cJulian     m_jdEpoch;
   cNoradBaseA* m_pNoradModel;

   std::string satName;
   int index;
   int epochYear;
   double epochDay;
   double meanMotion;
   double eccentricity;
   double inclination;
   double meanAnomaly;
   double raan;
   double argPerigee;
   double bstar;
   double drag;

   // Caching variables; note units are not necessarily the same as tle units
   mutable double m_secPeriod;

   // Caching variables recovered from the input TLE elements
   double m_aeAxisSemiMinorRec;  // semi-minor axis, in AE units
   double m_aeAxisSemiMajorRec;  // semi-major axis, in AE units
   double m_mnMotionRec;         // radians per minute
   double m_kmPerigeeRec;        // perigee, in km
   double m_kmApogeeRec;         // apogee, in km
};

#endif
