//-----------------------------------------------------
// cNoradBase.h
//
// This class provides a base class for the NORAD SGP4/SDP4
// orbit models.
//
// Copyright (c) 2003 Michael F. Henry
//-----------------------------------------------------
#ifndef __LIBNORAD_cNoradBaseA_H__
#define __LIBNORAD_cNoradBaseA_H__

class cEci;
class cOrbitA;

class cNoradBaseA
{
public:
   cNoradBaseA(const cOrbitA&);
   virtual ~cNoradBaseA(void);

   virtual bool getPosition(double tsince, cEci &eci) = 0;

protected:
   cNoradBaseA& operator=(const cNoradBaseA&);

   void Initialize();
   bool FinalPosition(double  incl, double omega,  double     e,
                      double     a, double    xl,  double xnode,
                      double    xn, double tsince, cEci &eci);

   const cOrbitA& m_Orbit;

   // Orbital parameter variables which need only be calculated one
   // time for a given orbit (ECI position time-independent).
   double m_satInc;  // inclination
   double m_satEcc;  // eccentricity

   double m_cosio;   double m_theta2;  double m_x3thm1;  double m_eosq;
   double m_betao2;  double m_betao;   double m_aodp;    double m_xnodp;
   double m_s4;      double m_qoms24;  double m_perigee; double m_tsi;
   double m_eta;     double m_etasq;   double m_eeta;    double m_coef;
   double m_coef1;   double m_c1;      double m_c2;      double m_c3;
   double m_c4;      double m_sinio;   double m_a3ovk2;  double m_x1mth2;
   double m_xmdot;   double m_omgdot;  double m_xhdot1;  double m_xnodot;
   double m_xnodcf;  double m_t2cof;   double m_xlcof;   double m_aycof;
   double m_x7thm1;
};

#endif
