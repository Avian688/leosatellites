package leosatellites.base;

import inet.mobility.contract.IMobility;
import leosatellites.mobility.NoradTLE;
import inet.node.inet.WirelessHost;
import inet.node.inet.AdhocHost;
import inet.node.inet.StandardHost;

// Basic Satellite model that uses the NoradTLE implementation, which does not allow the use of inter-satellite links; only ground relaying.
//
//
module BasicSatellite extends WirelessHost
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
        
        NoradModule: NoradTLE { // Module for transformation between omnet and libNorad
            parameters:
                @display("p=98,35");
        }
          
    connections allowunconnected:
}