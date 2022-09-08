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

package leosatellites.physicallayer.propagation;

import inet.physicallayer.wireless.common.base.packetlevel.PropagationBase;

//
// This propagation model computes the propagation time to be proportional to
// the traveled distance, the ratio is determined by the constant propagation
// speed parameter. A cartesian coordinate system are used to determine the distance
// so that distances are as accurate as possible for satellite/ground-station connections.
// Written by Aiden Valentine
//
module SatellitePropagation extends PropagationBase
{
    parameters:
        bool ignoreMovementDuringTransmission = default(true); // true means that the movement of the transmitter and the receiver during the signal transmission is ignored
        bool ignoreMovementDuringPropagation = default(true);  // true means that the movement of the transmitter and the receiver during the signal propagation is ignored
        bool ignoreMovementDuringReception = default(true);    // true means that the movement of the transmitter and the receiver during the signal reception is ignored
        @class(inet::physicallayer::SatellitePropagation);
}