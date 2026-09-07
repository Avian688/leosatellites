package leosatellites.visualizer.osg;
@namespace(inet);

simple LeoKPathOsgVisualizer
{
    parameters:
        string earthTexture;
        string routingDirectory;
        string snapshotSet;
        string policyLabel;
        string endpointPairs;
        int numSatellites;
        int numGroundStations;
        int numClients = default(0);
        int numUserTerminals;
        int satellitesPerPlane;
        int pathCount = default(10);
        int pairIndex = default(0);
        bool edgeDisjoint = default(false);
        int maxSharedLinks = default(-1);
        double maxRttSpread @unit(ms) = default(1000ms);
        double updateInterval @unit(s) = default(0.5s);
        double animationSpeed = default(1);
        bool autoCenterSelectedPair = default(true);
        double cameraLongitude = default(0);
        double cameraLatitude = default(20);
        double cameraDistance @unit(m) = default(26000000m);
        bool showSatellites = default(true);
        bool showInterSatelliteLinks = default(true);
        bool showGroundStations = default(true);
        bool showUserTerminals = default(true);
        double satellitePointSize = default(10); // screen pixels
        double groundStationPointSize = default(15);
        double userTerminalPointSize = default(15);
        double selectedEndpointPointSize = default(20);
        double groundMarkerAltitude @unit(m) = default(15000m);
        double terminalMarkerAltitude @unit(m) = default(30000m);
        @display("i=misc/globe");
}
