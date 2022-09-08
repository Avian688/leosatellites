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

package leosatellites.physicallayer.unitdisk;

import inet.physicallayer.wireless.unitdisk.UnitDiskTransmitter;

//-----------------------------------------------------
// This model creates a SatelliteUnitDiskTransmission which stores the longitude, latitude and altitude of a given transmitter.
// These values are used within the path loss and propagation calculations so that the visual x, y and z values can be
// decoupled from their actual positions.
// Written by Aiden Valentine
//-----------------------------------------------------

module SatelliteUnitDiskTransmitter extends UnitDiskTransmitter
{
    parameters:
        @class(inet::physicallayer::SatelliteUnitDiskTransmitter);
        @display("i=block/wtx");
}