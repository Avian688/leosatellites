//-----------------------------------------------------
// cNoradSGP4A.cpp
//
// NORAD SGP4 implementation. See historical note in cNoradBase.cpp
// Copyright (c) 2003 Michael F. Henry
// Updated for use with models implemented by Aiden Valentine
//
// mfh 12/07/2003
//-----------------------------------------------------
#include "cNoradSGP4A.h"

#include  <cmath>

#include "cOrbitA.h"
#include "libnorad/cJulian.h"
#include "libnorad/cVector.h"
#include "libnorad/ccoord.h"

// This class implements the NORAD Simple General Perturbation 4 orbit
// model. This model provides the ECI coordiantes/velocity of satellites
// with orbit periods less than 225 minutes. Unlike the standard cNoradSGP4 class
// this implementation does not use TLE data sets and instead uses Keplerian elements
// that can be defined by the user for dynamic satellite constellation creation.

cNoradSGP4A::cNoradSGP4A(const cOrbitA& orbit) :
   cNoradBaseA(orbit)
{
   m_c5     = 2.0 * m_coef1 * m_aodp * m_betao2 *
              (1.0 + 2.75 * (m_etasq + m_eeta) + m_eeta * m_etasq);
   m_omgcof = m_Orbit.BStar() * m_c3 * std::cos(m_Orbit.ArgPerigee());
   m_xmcof  = -TWOTHRD * m_coef * m_Orbit.BStar() * AE / m_eeta;
   m_delmo  = std::pow(1.0 + m_eta * std::cos(m_Orbit.mnAnomaly()), 3.0);
   m_sinmo  = std::sin(m_Orbit.mnAnomaly());
}

cNoradSGP4A::~cNoradSGP4A()
{}

//-----------------------------------------------------
// getPosition()
// This procedure returns the ECI position and velocity for the satellite
// in the orbit at the given number of minutes since the TLE epoch time
// using the NORAD Simplified General Perturbation 4, near earth orbit
// model.
//
// tsince - Time in minutes since the TLE epoch (GMT).
// eci    - ECI object to hold position information.
//           To convert the returned ECI position vector to km,
//           multiply each component by:
//              (XKMPER_WGS72 / AE).
//           To convert the returned ECI velocity vector to km/sec,
//           multiply each component by:
//              (XKMPER_WGS72 / AE) * (MIN_PER_DAY / 86400).
//-----------------------------------------------------
bool cNoradSGP4A::getPosition(double tsince, cEci& eci)
{
   // For m_perigee less than 220 kilometers, the isimp flag is set and
   // the equations are truncated to linear variation in sqrt a and
   // quadratic variation in mean anomaly.  Also, the m_c3 term, the
   // delta omega term, and the delta m term are dropped.
   bool isimp = false;
   if ((m_aodp * (1.0 - m_satEcc) / AE) < (220.0 / XKMPER_WGS72 + AE)) {
      isimp = true;
   }

   double d2 = 0.0;
   double d3 = 0.0;
   double d4 = 0.0;

   double t3cof = 0.0;
   double t4cof = 0.0;
   double t5cof = 0.0;

   if (!isimp) {
      double c1sq = m_c1 * m_c1;

      d2 = 4.0 * m_aodp * m_tsi * c1sq;

      const double temp = d2 * m_tsi * m_c1 / 3.0;

      d3 = (17.0 * m_aodp + m_s4) * temp;
      d4 = 0.5 * temp * m_aodp * m_tsi *
           (221.0 * m_aodp + 31.0 * m_s4) * m_c1;
      t3cof = d2 + 2.0 * c1sq;
      t4cof = 0.25 * (3.0 * d3 + m_c1 * (12.0 * d2 + 10.0 * c1sq));
      t5cof = 0.2 * (3.0 * d4 + 12.0 * m_c1 * d3 + 6.0 *
                     d2 * d2 + 15.0 * c1sq * (2.0 * d2 + c1sq));
   }

   // Update for secular gravity and atmospheric drag.
   const double xmdf   = m_Orbit.mnAnomaly() + m_xmdot * tsince;
   const double omgadf = m_Orbit.ArgPerigee() + m_omgdot * tsince;
   const double xnoddf = m_Orbit.RAAN() + m_xnodot * tsince;
   double omega  = omgadf;
   double xmp    = xmdf;
   const double tsq    = tsince * tsince;
   double xnode  = xnoddf + m_xnodcf * tsq;
   double tempa  = 1.0 - m_c1 * tsince;
   double tempe  = m_Orbit.BStar() * m_c4 * tsince;
   double templ  = m_t2cof * tsq;

   if (!isimp) {
      double delomg = m_omgcof * tsince;
      double delm = m_xmcof * (std::pow(1.0 + m_eta * std::cos(xmdf), 3.0) - m_delmo);
      double temp = delomg + delm;

      xmp = xmdf + temp;
      omega = omgadf - temp;

      double tcube = tsq * tsince;
      double tfour = tsince * tcube;

      tempa = tempa - d2 * tsq - d3 * tcube - d4 * tfour;
      tempe = tempe + m_Orbit.BStar() * m_c5 * (std::sin(xmp) - m_sinmo);
      templ = templ + t3cof * tcube + tfour * (t4cof + tsince * t5cof);
   }

   const double a  = m_aodp * sqr(tempa);
   const double e  = m_satEcc - tempe;


   const double xl = xmp + omega + xnode + m_xnodp * templ;
   const double xn = XKE / pow(a, 1.5);

   return FinalPosition(m_satInc, omgadf, e, a, xl, xnode, xn, tsince, eci);
}

