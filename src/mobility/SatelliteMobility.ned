package leosatellites.mobility;

import os3.mobility.SatSGP4Mobility;

//
// Manages the SatSGP4 Mobility - extension of OS3 mobility model
//
simple SatelliteMobility extends SatSGP4Mobility
{
    parameters:
        @class("inet::SatelliteMobility");
}