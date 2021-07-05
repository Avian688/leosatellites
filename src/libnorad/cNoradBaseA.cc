//-----------------------------------------------------
// cNoradBaseA.cpp
//
// Historical Note:
// The equations used here (and in derived classes) to determine satellite
// ECI coordinates/velocity come from the December, 1980 NORAD document
// "Space Track Report No. 3". The report details 6 orbital models and
// provides FORTRAN IV implementations of each. The classes here
// implement only two of the orbital models: SGP4 and SDP4. These two models,
// one for "near-earth" objects and one for "deep space" objects, are widely
// used in satellite tracking software and can produce very accurate results
// when used with current NORAD two-line element datum.
//
// The NORAD FORTRAN IV SGP4/SDP4 implementations were converted to Pascal by
// Dr. TS Kelso in 1995. In 1996 these routines were ported in a straight-
// forward manner to C++ by Varol Okan. The SGP4/SDP4 classes here were
// written by Michael F. Henry in 2002-03 and are a modern C++ re-write of
// the work done by Okan. In addition to introducing an object-oriented
// architecture, the last residues of the original FORTRAN code (such as
// labels and gotos) were eradicated.
//
// For excellent information on the underlying physics of orbits, visible
// satellite observations, current NORAD TLE data, and other related material,
// see http://www.celestrak.com which is maintained by Dr. TS Kelso.
//
// The code has been adapted to use the new cOrbitA class which does not require
// the use of TLE data sets so that Keplerian elements can be used instead.
// Copyright (c) 2003 Michael F. Henry - Adapted by Aiden Valentine
//
//-----------------------------------------------------
#include "cNoradBaseA.h"
#include "cOrbitA.h"
#include "libnorad/ccoord.h"
#include "libnorad/cEci.h"
#include "libnorad/cVector.h"
#include "libnorad/cJulian.h"

#include <cmath>

cNoradBaseA::cNoradBaseA(const cOrbitA& orbit) :
   m_Orbit(orbit)
{
   Initialize();
}

cNoradBaseA::~cNoradBaseA()
{}

cNoradBaseA& cNoradBaseA::operator=(const cNoradBaseA& b)
{
   // m_Orbit is a "const" member var, so cast away its
   // "const-ness" in order to complete the assigment.
   *(const_cast<cOrbitA*>(&m_Orbit)) = b.m_Orbit;

   return *this;
}

//////////////////////////////////////////////////////////////////////////////
// Initialize()
// Perform the initialization of member variables, specifically the variables
// used by derived-class objects to calculate ECI coordinates.
void cNoradBaseA::Initialize()
{
   // Initialize any variables which are time-independent when
   // calculating the ECI coordinates of the satellite.
   m_satInc = m_Orbit.Inclination();
   m_satEcc = m_Orbit.Eccentricity();

   m_cosio  = std::cos(m_satInc);
   m_theta2 = m_cosio * m_cosio;
   m_x3thm1 = 3.0 * m_theta2 - 1.0;
   m_eosq   = m_satEcc * m_satEcc;
   m_betao2 = 1.0 - m_eosq;
   m_betao  = std::sqrt(m_betao2);

   // The "recovered" semi-minor axis and mean motion.
   m_aodp  = m_Orbit.SemiMinor();
   m_xnodp = m_Orbit.mnMotionRec();

   // For perigee below 156 km, the values of S and QOMS2T are altered.
   m_perigee = XKMPER_WGS72 * (m_aodp * (1.0 - m_satEcc) - AE);

   m_s4      = SV;
   m_qoms24  = QOMS2T;

   if (m_perigee < 156.0) {
      m_s4 = m_perigee - 78.0;

      if (m_perigee <= 98.0) {
         m_s4 = 20.0;
      }

      m_qoms24 = std::pow((120.0 - m_s4) * AE / XKMPER_WGS72, 4.0);
      m_s4 = m_s4 / XKMPER_WGS72 + AE;
   }

   const double pinvsq = 1.0 / (m_aodp * m_aodp * m_betao2 * m_betao2);

   m_tsi   = 1.0 / (m_aodp - m_s4);
   m_eta   = m_aodp * m_satEcc * m_tsi;
   m_etasq = m_eta * m_eta;
   m_eeta  = m_satEcc * m_eta;

   const double psisq  = std::fabs(1.0 - m_etasq);

   m_coef  = m_qoms24 * std::pow(m_tsi,4.0);
   m_coef1 = m_coef / std::pow(psisq,3.5);

   const double c2 = m_coef1 * m_xnodp *
                     (m_aodp * (1.0 + 1.5 * m_etasq + m_eeta * (4.0 + m_etasq)) +
                     0.75 * CK2 * m_tsi / psisq * m_x3thm1 *
                     (8.0 + 3.0 * m_etasq * (8.0 + m_etasq)));

   m_c1    = m_Orbit.BStar() * c2;
   m_sinio = std::sin(m_satInc);

   const double a3ovk2 = -XJ3 / CK2 * std::pow(AE,3.0);

   m_c3     = m_coef * m_tsi * a3ovk2 * m_xnodp * AE * m_sinio / m_satEcc;
   m_x1mth2 = 1.0 - m_theta2;
   m_c4     = 2.0 * m_xnodp * m_coef1 * m_aodp * m_betao2 *
              (m_eta * (2.0 + 0.5 * m_etasq) +
              m_satEcc * (0.5 + 2.0 * m_etasq) -
              2.0 * CK2 * m_tsi / (m_aodp * psisq) *
              (-3.0 * m_x3thm1 * (1.0 - 2.0 * m_eeta + m_etasq * (1.5 - 0.5 * m_eeta)) +
              0.75 * m_x1mth2 *
              (2.0 * m_etasq - m_eeta * (1.0 + m_etasq)) *
              std::cos(2.0 * m_Orbit.ArgPerigee())));

   const double theta4 = m_theta2 * m_theta2;
   const double temp1  = 3.0 * CK2 * pinvsq * m_xnodp;
   const double temp2  = temp1 * CK2 * pinvsq;
   const double temp3  = 1.25 * CK4 * pinvsq * pinvsq * m_xnodp;

   m_xmdot = m_xnodp + 0.5 * temp1 * m_betao * m_x3thm1 +
             0.0625 * temp2 * m_betao *
             (13.0 - 78.0 * m_theta2 + 137.0 * theta4);

   const double x1m5th = 1.0 - 5.0 * m_theta2;

   m_omgdot = -0.5 * temp1 * x1m5th + 0.0625 * temp2 *
              (7.0 - 114.0 * m_theta2 + 395.0 * theta4) +
              temp3 * (3.0 - 36.0 * m_theta2 + 49.0 * theta4);

   const double xhdot1 = -temp1 * m_cosio;

   m_xnodot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * m_theta2) +
              2.0 * temp3 * (3.0 - 7.0 * m_theta2)) * m_cosio;
   m_xnodcf = 3.5 * m_betao2 * xhdot1 * m_c1;
   m_t2cof  = 1.5 * m_c1;
   m_xlcof  = 0.125 * a3ovk2 * m_sinio *
              (3.0 + 5.0 * m_cosio) / (1.0 + m_cosio);
   m_aycof  = 0.25 * a3ovk2 * m_sinio;
   m_x7thm1 = 7.0 * m_theta2 - 1.0;
}

bool cNoradBaseA::FinalPosition(double incl, double  omega,
                               double    e, double      a,
                               double   xl, double  xnode,
                               double   xn, double tsince,
                               cEci &eci)
{
   if ((e * e) > 1.0) {
      // error in satellite data
      return false;
   }

   const double beta = std::sqrt(1.0 - e * e);

   // Long period periodics
   const double axn  = e * std::cos(omega);
   double temp = 1.0 / (a * beta * beta);
   const double xll  = temp * m_xlcof * axn;
   const double aynl = temp * m_aycof;
   const double xlt  = xl + xll;
   const double ayn  = e * std::sin(omega) + aynl;

   // Solve Kepler's Equation

   const double capu   = Fmod2p(xlt - xnode);
   double temp2  = capu;
   double temp3  = 0.0;
   double temp4  = 0.0;
   double temp5  = 0.0;
   double temp6  = 0.0;
   double sinepw = 0.0;
   double cosepw = 0.0;
   bool   fDone  = false;

   for (int i = 1; (i <= 10) && !fDone; i++) {
      sinepw = std::sin(temp2);
      cosepw = std::cos(temp2);
      temp3 = axn * sinepw;
      temp4 = ayn * cosepw;
      temp5 = axn * cosepw;
      temp6 = ayn * sinepw;

      double epw = (capu - temp4 + temp3 - temp2) /
                   (1.0 - temp5 - temp6) + temp2;

      if (std::fabs(epw - temp2) <= E6A)
         fDone = true;
      else
         temp2 = epw;
   }

   // Short period preliminary quantities
   const double ecose = temp5 + temp6;
   const double esine = temp3 - temp4;
   const double elsq  = axn * axn + ayn * ayn;
   temp  = 1.0 - elsq;
   const double pl = a * temp;
   const double r  = a * (1.0 - ecose);
   double temp1 = 1.0 / r;
   const double rdot  = XKE * std::sqrt(a) * esine * temp1;
   const double rfdot = XKE * std::sqrt(pl) * temp1;
   temp2 = a * temp1;
   const double betal = std::sqrt(temp);
   temp3 = 1.0 / (1.0 + betal);
   const double cosu  = temp2 * (cosepw - axn + ayn * esine * temp3);
   const double sinu  = temp2 * (sinepw - ayn - axn * esine * temp3);
   const double u     = AcTan(sinu, cosu);
   const double sin2u = 2.0 * sinu * cosu;
   const double cos2u = 2.0 * cosu * cosu - 1.0;

   temp  = 1.0 / pl;
   temp1 = CK2 * temp;
   temp2 = temp1 * temp;

   // Update for short periodics
   const double rk = r * (1.0 - 1.5 * temp2 * betal * m_x3thm1) +
                     0.5 * temp1 * m_x1mth2 * cos2u;
   const double uk = u - 0.25 * temp2 * m_x7thm1 * sin2u;
   const double xnodek = xnode + 1.5 * temp2 * m_cosio * sin2u;
   const double xinck  = incl + 1.5 * temp2 * m_cosio * m_sinio * cos2u;
   const double rdotk  = rdot - xn * temp1 * m_x1mth2 * sin2u;
   const double rfdotk = rfdot + xn * temp1 * (m_x1mth2 * cos2u + 1.5 * m_x3thm1);

   // Orientation vectors
   const double sinuk  = std::sin(uk);
   const double cosuk  = std::cos(uk);
   const double sinik  = std::sin(xinck);
   const double cosik  = std::cos(xinck);
   const double sinnok = std::sin(xnodek);
   const double cosnok = std::cos(xnodek);
   const double xmx = -sinnok * cosik;
   const double xmy = cosnok * cosik;
   const double ux  = xmx * sinuk + cosnok * cosuk;
   const double uy  = xmy * sinuk + sinnok * cosuk;
   const double uz  = sinik * sinuk;
   const double vx  = xmx * cosuk - cosnok * sinuk;
   const double vy  = xmy * cosuk - sinnok * sinuk;
   const double vz  = sinik * cosuk;

   // Position
   const double x = rk * ux;
   const double y = rk * uy;
   const double z = rk * uz;

   cVector vecPos(x, y, z);

   // Validate on altitude
   const double altKm = (vecPos.Magnitude() * (XKMPER_WGS72 / AE));

   if ((altKm < XKMPER_WGS72) || (altKm > (2 * GEOSYNC_ALT)))
      return false;

   // Velocity
   const double xdot = rdotk * ux + rfdotk * vx;
   const double ydot = rdotk * uy + rfdotk * vy;
   const double zdot = rdotk * uz + rfdotk * vz;

   cVector vecVel(xdot, ydot, zdot);

   cJulian gmt = m_Orbit.Epoch();
   gmt.addMin(tsince);

   eci = cEci(vecPos, vecVel, gmt);

   return true;
}



