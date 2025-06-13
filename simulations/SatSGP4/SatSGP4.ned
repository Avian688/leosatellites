package leosatellites.simulations.SatSGP4;

import leosatellites.base.LaserSatellitePPP;
import leosatellites.base.GroundStationPPP;
import os3.base.MissionControlCenter;
import inet.physicallayer.wireless.common.contract.packetlevel.IRadioMedium;
import inet.physicallayer.wireless.apsk.packetlevel.ApskScalarRadioMedium;
import os3.base.CNI_OS3;
import leosatellites.networklayer.configurator.ipv4.LeoIpv4NetworkConfigurator;
import inet.networklayer.configurator.ipv4.Ipv4FlatNetworkConfigurator;
import inet.visualizer.contract.IIntegratedVisualizer;
import inet.node.inet.WirelessHost;
import inet.node.contract.INetworkNode;

import inet.common.geometry.common.SimpleGeographicCoordinateSystem;
import inet.common.geometry.common.OsgGeographicCoordinateSystem;
import inet.environment.objectcache.GridObjectCache;

import leosatellites.physicallayer.unitdisk.SatelliteUnitDiskTransmitter;
import leosatellites.physicallayer.propagation.SatellitePropagation;

import leosatellites.common.LeoChannelConstructor;
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
        int numOfGS; // Number of Mission Control Centers
        double incl;
        double alt;
        int satsPerPlane;
        int numOfPlanes;
        bool enableInterSatelliteLinks = default(true);
        
        @display("bgi=background_earth,s;bgb=2160,1080");
        //@statistic[rtt](source=count(GS[0].app[0].rtt); record=figure; targetFigure=rttCounter; checkSignals=false);
        //@figure[rttCounter](type=plot; pos=600,50; label="Round Trip Time");
    submodules:
        visualizer: <default(firstAvailableOrEmpty("IntegratedCanvasVisualizer"))> like IIntegratedVisualizer if typename != "" {
            parameters:
                @display("p=100,400;is=s");
        }
        configurator: LeoIpv4NetworkConfigurator {
            @display("p=183.94,67.625");
        }
        satellite[numOfSats]: LaserSatellitePPP {   //LaserSatellite for using NoradA, Satellite for Ground-Relays using TLE
            parameters:
                @display("p=238.04,327.305;i=satellit_blue");
                NoradModule.satPerPlane = parent.satsPerPlane;
                NoradModule.planes = parent.numOfPlanes;
        }
        groundStation[numOfGS]: GroundStationPPP {
            parameters:
                @display("p=238.04,343.535;r=10;i=device/receiverdish");
        }
        channelConstructor: LeoChannelConstructor  {
        	@display("p=704.06,601.0725;i=abstract/person,#17FF39;b=,,rect;is=vl;t=Channel Constructor");
        }
    connections allowunconnected:
}