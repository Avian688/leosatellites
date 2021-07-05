
package leosatellites.base;

import inet.mobility.contract.IMobility;
import os3.mobility.Norad;
import leosatellites.mobility.NoradA;
import inet.node.inet.WirelessHost;
import inet.node.inet.AdhocHost;
import inet.node.inet.StandardHost;

// Laser Satellite model that uses the NoradA implementation, which therefore allows the use of inter-satellite links.
//
//
module LaserSatellite extends WirelessHost
{
    parameters:
        @node; //because of MobilityBase initialization
        @networkNode;
        string satelliteName = default(""); // Parameter for satellite name
        string mobilityType = default("SatSGP4Mobility"); // Define mobility module
        // Transmit Power of satellite (7dBW as example for CospasSarsat system)
        double transmitPower @unit(dBW) @display("i=device/satellite;bgb=324,226") = default(7dBW); // Symbol
        @display("t=$satelliteName");     
    submodules:
        //mobility : SatSGP4Mobility {
        //     parameters:
        //     @display("p=194,103");
        //}
        
        NoradModule: NoradA { // Module for transformation between omnet and libNorad
            parameters:
                @display("p=98,35");
        }
          
    connections allowunconnected:
}
