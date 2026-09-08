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

package leosatellites.networklayer.ipv4;
import inet.networklayer.ipv4.Ipv4;
simple LeoIpv4 extends Ipv4
{
    // Opt-in experiment mapping. The base connection uses rank 1; subflow
    // port pair (clientPort+i, serverPort+i) uses rank i, in both directions.
    // Requires configurator.kPathPingRouting and loaded K-path snapshots.
    int kPathTcpPathGroup = default(-1); // -1 leaves ordinary TCP routing unchanged
    int kPathTcpClientPort = default(4000);
    int kPathTcpServerPort = default(1000);
    int kPathTcpSubflows = default(1);
    @class(inet::LeoIpv4);
}
