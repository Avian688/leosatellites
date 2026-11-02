package leosatellites.simulations.examples.routing;
@namespace(inet);

import leosatellites.mobility.NoradA;
import leosatellites.visualizer.osg.LeoKPathOsgVisualizer;

module KPathOsgSatellite
{
    parameters:
        int satIndex;
        string satelliteName = default("Starlink Satellite");
        double altitude;
        double inclination;
        int phaseOffset = default(0);
        int planes;
        int satPerPlane;
        @display("i=device/satellite");
    submodules:
        NoradModule: NoradA {
            parameters:
                satIndex = parent.satIndex;
                satName = parent.satelliteName;
                altitude = parent.altitude;
                inclination = parent.inclination;
                phaseOffset = parent.phaseOffset;
                planes = parent.planes;
                satPerPlane = parent.satPerPlane;
                @display("p=80,80");
        }
}

module KPathOsgGeoNode
{
    parameters:
        double latitude;
        double longitude;
        string label;
        @display("i=device/receiverdish");
}

network LeoRoutingOsg
{
    parameters:
        int numOfSats = default(1584);
        int numOfGS = default(100);
        int numOfClients = default(0);
        int numOfUserTerminals = default(10);
        double incl = default(53);
        double alt = default(550);
        int satsPerPlane = default(22);
        int numOfPlanes = default(72);
        @display("bgb=1200,700");
    submodules:
        satellite[numOfSats]: KPathOsgSatellite {
            parameters:
                satIndex = index;
                altitude = parent.alt;
                inclination = parent.incl * 0.0174532925199433;
                planes = parent.numOfPlanes;
                satPerPlane = parent.satsPerPlane;
                @display("p=100,100");
        }
        groundStation[numOfGS]: KPathOsgGeoNode {
            @display("p=300,100");
        }
        userTerminal[numOfUserTerminals]: KPathOsgGeoNode {
            @display("p=500,100");
        }
        pathVisualizer: LeoKPathOsgVisualizer {
            parameters:
                numSatellites = parent.numOfSats;
                numGroundStations = parent.numOfGS;
                numClients = parent.numOfClients;
                numUserTerminals = parent.numOfUserTerminals;
                satellitesPerPlane = parent.satsPerPlane;
                @display("p=700,100");
        }
    connections allowunconnected:
}
