package leosatellites.mobility;

simple NoradTLE like INorad
{
parameters:
    string TLEfile = default("");          // filename of TLE data file
    double elevationAngle = default(25);
    @display("i=msg/book");
}