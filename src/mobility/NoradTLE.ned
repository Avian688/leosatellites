package leosatellites.mobility;

simple NoradTLE like INorad
{
parameters:
    string TLEfile = default("");          // filename of TLE data file
    @display("i=msg/book");
}