
package leosatellites.mobility;


// This NED file details the NoradA model which does not use a TLE file so that
// inter-satellite links can be implemented as well as to allow the simulation
// of any constellation not bounded by a TLE file. All of the Keplerian parameters
// below are representative of an average of some Starlink satellites. These can be easily
// adapted depending on the objective of the simulation.
simple NoradA like INorad
{
parameters:
    int satIndex;
    string satName;
    int epochYear = default(21);
    double epochDay = default(86.49033611);
    //double meanMotion = default(15.15306312100295);    //consistent between satellites
    double altitude = default(550); //km
    double eccentricity = default(0.0001698);
    double inclination = default(53*0.0174533); 	  //consistent between satellites
    double meanAnomaly = default(0);
    double bstar = default(0.00042243);
    double drag = default(0.00001846);
	int phaseOffset = default(0);  //Must be lower than the number of planes. E.g. a phaseOffset of 5 with 32 orbital planes would equate to 5/32.
	int planes = default(1);
	int satPerPlane = default(1);
    @display("i=msg/book");
}
