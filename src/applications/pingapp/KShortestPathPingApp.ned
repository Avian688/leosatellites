package leosatellites.applications.pingapp;

import inet.applications.pingapp.PingApp;

simple KShortestPathPingApp extends PingApp
{
    parameters:
        @class(inet::KShortestPathPingApp);
        int pathGroup; // Index in the configurator's sorted endpoint-pair list
        int pathIndex; // One-based rank in the selected K-path policy
        bool monitorOnly = default(false); // Sample catalog state without injecting probe traffic; use count=-1
        @signal[kPathAvailable](type=long);
        @signal[kPathExpectedRtt](type=double; unit=s);
        @signal[kPathCoreLinkCount](type=long);
        @signal[kPathCatalogSize](type=long);
        @statistic[kPathAvailable](record=vector; interpolationmode=sample-hold);
        @statistic[kPathExpectedRtt](record=vector; unit=s; interpolationmode=sample-hold);
        @statistic[kPathCoreLinkCount](record=vector; interpolationmode=sample-hold);
        @statistic[kPathCatalogSize](record=vector; interpolationmode=sample-hold);
}
