package leosatellites.simulations.examples.routing;
@namespace(inet);

import inet.visualizer.contract.IIntegratedVisualizer;
import leosatellites.base.GroundStationPPP;
import leosatellites.base.LaserSatellitePPP;
import leosatellites.base.UserTerminal;
import leosatellites.common.LeoChannelConstructor;
import leosatellites.common.UserTerminalHandoverOracle;
import leosatellites.networklayer.configurator.ipv4.LeoIpv4NetworkConfigurator;

network LeoRoutingNetwork
{
    parameters:
        int numOfSats;
        int numOfGS;
        int numOfClients = default(0);
        int numOfUserTerminals = default(0);
        double incl;
        double alt;
        int satsPerPlane;
        int numOfPlanes;
        bool enableInterSatelliteLinks = default(true);
        @display("bgi=background_earth,s;bgb=2160,1080");
    submodules:
        visualizer: <default("")> like IIntegratedVisualizer if typename != "" {
            parameters:
                @display("p=100,400;is=s");
        }
        configurator: LeoIpv4NetworkConfigurator {
            parameters:
                @display("p=184,68");
        }
        satellite[numOfSats]: LaserSatellitePPP {
            parameters:
                NoradModule.satPerPlane = parent.satsPerPlane;
                NoradModule.planes = parent.numOfPlanes;
                @display("p=238,327;i=satellit_blue");
        }
        groundStation[numOfGS]: GroundStationPPP {
            parameters:
                @display("p=238,344;r=10;i=device/receiverdish");
        }
        channelConstructor: LeoChannelConstructor {
            parameters:
                @display("p=2010,979;i=abstract/person,#17FF39;t=Channel Constructor");
        }
        handoverOracle: UserTerminalHandoverOracle {
            parameters:
                @display("p=2001,979;i=abstract/person,#17FF39;t=User Terminal Oracle");
        }
        userTerminal[numOfUserTerminals]: UserTerminal {
            parameters:
                @display("p=460,251,m,n,numOfUserTerminals;r=10;i=device/laptop");
        }
    connections allowunconnected:
}
