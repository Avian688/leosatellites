//
// cNoradSGP4A.h
//
// This class implements the NORAD Simple General Perturbation 4 orbit
// model. This model provides the ECI coordiantes/velocity of satellites
// with orbit periods less than 225 minutes. Unlike the standard cNoradSGP4 class
// this implementation does not use TLE data sets and instead uses Keplerian elements
// that can be defined by the user for dynamic satellite constellation creation.
//
// Copyright (c) 2003 Michael F. Henry - Adapted by Aiden Valentine
//
#ifndef __LIBNORAD_cNoradSGP4A_H__
#define __LIBNORAD_cNoradSGP4A_H__

#include "cNoradBaseA.h"

class cOrbitA;

class cNoradSGP4A : public cNoradBaseA
{
public:
   cNoradSGP4A(const cOrbitA& orbit);
   virtual ~cNoradSGP4A();

   virtual bool getPosition(double tsince, cEci& eci);

protected:
   double m_c5;
   double m_omgcof;
   double m_xmcof;
   double m_delmo;
   double m_sinmo;
};

#endif
