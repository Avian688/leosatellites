//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package leosatellites.networklayer.configurator.ipv4;

import inet.networklayer.configurator.base.L3NetworkConfiguratorBase;

simple LeoIpv4NetworkConfigurator extends L3NetworkConfiguratorBase
{
    parameters:
        @class(inet::LeoIpv4NetworkConfigurator);
        @display("i=block/cogwheel");
        int numOfKPaths = default(1); // Maximum paths saved and returned by the endpoint path catalog
        bool kPathsEdgeDisjoint = default(false); // Disjointness applies to the satellite/ground-station core, not endpoint access links
        double kPathMaxRttSpread @unit(ms) = default(5ms); // Keep paths no more than this much slower than the shortest RTT
        string kPathSnapshotMode = default("disabled"); // disabled, generate, or load
        string kPathSnapshotSet = default(""); // Named endpoint geometry/path set under the routing corpus kpaths directory
        string kPathEndpointPairs = default("all"); // all end-host pairs, or comma-separated endpointA->endpointB module names
        bool loadFiles = default (true);
        bool allowRouteSnapshotOverwrite = default(false); // Explicit opt-in for regenerating an existing snapshot path
        bool logRouteSnapshotStats = default(false); // Print/read counters for focused benchmarks
        
        string configLocation = default (""); // Routing corpus root; constellation folder and timestamp names are unchanged
}
