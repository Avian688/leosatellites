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

simple LeoFlowScheduler
{
    parameters:
        @class("inet::LeoFlowScheduler");
        bool isWebSearchWorkLoad;
    	string transportProtocol;
        string clientAppType;
        string receiverAppType;
        //  @dynamic(true);
        string trafficMatrixType;
        int numOfGS;
        int arrivalRate; //= default(4);
        //        int flowSize @unit(B); //= default(4);
        //        double longFlowSize @unit(B)=  default(10MiB);
        int switchQueueLength; // initial windows size
        bool perFlowEcmp;
        bool perPacketEcmp;

        int seedValue;
        int flowSize;
        int longFlowSize;
        volatile int numRunningShortFlowsNow = default(0);
        int numShortFlows;
        double percentLongFlowNodes;
        volatile int numCompletedShortFlows = default(0);
        volatile int numCompletedLongFlows = default(0);
        volatile int numTimesDecodingSucceeded = default(0);
        volatile int numTimesDecodingFailed = default(0);

         //  @unit(s) = default(1s);
        bool shuffle;
        bool randomGroup;
}
