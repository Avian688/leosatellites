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

package leosatellites.base;
import leosatellites.mobility.GroundStationMobility;
import inet.node.inet.StandardHost;

module GroundStationPPP extends StandardHost//WirelessHost
{
    parameters:
        string cityName = default("");
        string mobilityType = default("GroundStationMobility"); // Define mobility module
        @display("t=$cityName");
        @node;

//    gates:
//        input radioIn; // Input gate
//        output radioOut; // Output gate

    connections allowunconnected: // Connections between compound gates and submodules

}