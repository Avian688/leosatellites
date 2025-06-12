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

package leosatellites.common;

simple LeoChannelConstructor
{
    parameters:
	   @class("inet::LeoChannelConstructor");
	   string addressBase = default("10.0.0.0");    // start of address range from which to automatically assign an address to the autoassignInterfaces
       string netmask = default("255.0.0.0"); 
       double dataRate @unit(bps) = default(10Mbps);
       int queueSize = default(1000);
       string interfaceType = default("leosatellites.linklayer.ppp.PppInterfaceMutable"); //Required if you need different queues.
}