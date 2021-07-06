package leosatellites.simulations.SatSGP4;

import leosatellites.base.BasicSatellite;
import leosatellites.base.LaserSatellite;
import leosatellites.base.GroundStation;
import os3.base.MissionControlCenter;
import inet.physicallayer.contract.packetlevel.IRadioMedium;
import inet.physicallayer.apskradio.packetlevel.ApskScalarRadioMedium;
import os3.base.CNI_OS3;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import leosatellites.networklayer.configurator.ipv4.SatelliteNetworkConfigurator;
import inet.networklayer.configurator.ipv4.Ipv4FlatNetworkConfigurator;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.node.inet.WirelessHost;
import inet.node.inet.INetworkNode;

import inet.common.geometry.common.SimpleGeographicCoordinateSystem;
import inet.common.geometry.common.OsgGeographicCoordinateSystem;
import inet.environment.objectcache.GridObjectCache;

import leosatellites.physicallayer.unitdisk.SatelliteUnitDiskTransmitter;
import leosatellites.physicallayer.propagation.SatellitePropagation;
//
// Network SatSGP4 enables satellite movement on worldmap. This is the primary satellite network that
// is testing throughout the project by Aiden Valentine. The network has been largely adpated from the
// Initial OS3 implementation so that it works with the latest version of INET. The NED file below illustrates
// the new mdodels being used such as the SatelliteNetworkConfigurator which will periodically run Dijkstra's
// algorthm to re establish routes in a dynamic satellite network.
//
network SatSGP4
{
    parameters:
        int numOfSats; // Number of satellites
        int numOfMCCs; // Number of Mission Control Centers
        @display("bgi=background_earth,s;bgb=2160,1080");
        //@statistic[rtt](source=count(MCC[0].app[0].rtt); record=figure; targetFigure=rttCounter; checkSignals=false);
        //@figure[rttCounter](type=plot; pos=600,50; label="Round Trip Time");
    submodules:
        visualizer: <default("IntegratedCanvasVisualizer")> like IIntegratedVisualizer if hasVisualizer() {
            @display("p=305.665,67.625`");
        }
        configurator: SatelliteNetworkConfigurator {
            @display("p=183.94,67.625");
        }
        satellite[numOfSats]: LaserSatellite {   //LaserSatellite for using NoradA, Satellite for Ground-Relays using TLE
            parameters:
                @display("p=238.04,327.305;i=satellit_blue");
        }
        MCC[numOfMCCs]: GroundStation {
            parameters:
                @display("p=238.04,343.535;r=10;i=device/receiverdish");
        }
        radioMedium: <default("UnitDiskRadioMedium")> like IRadioMedium {
            //radioMedium: ApskScalarRadioMedium {
            parameters:
                //numChannels = 2;
                @display("p=421.98,64.92;i=misc/sun");
        }
    connections allowunconnected:
}