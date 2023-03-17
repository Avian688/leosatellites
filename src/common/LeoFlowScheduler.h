#ifndef __LEO_LeoFlowScheduler_H
#define __LEO_LeoFlowScheduler_H
#include <chrono>  // for high_resolution_clock

#include <map>
#include <set>
#include <algorithm>    // std::random_shuffle
#include <vector>       // std::vector
#include <random>
#include <cmath>
#include <time.h>       /* time */

#include "inet/common/INETDefs.h"
#include "inet/common/lifecycle/ILifecycle.h"

namespace inet {

enum TransportProtocol
{
    TCP
};

class INET_API LeoFlowScheduler : public cSimpleModule, public ILifecycle
{
//private:
protected:

    bool isWebSearchWorkLoad;
    unsigned int indexWorkLoad;
    std::vector<unsigned int> flowSizeWebSeachWorkLoad;

    std::chrono::high_resolution_clock::time_point t1;
    std::chrono::high_resolution_clock::time_point t2;
    simtime_t totalSimTime;
    cOutVector permMapLongFlowsVec;
    cOutVector permMapShortFlowsVec;

    cOutVector randMapShortFlowsVec;
    cOutVector permMapShortFlowsVector;

    cOutVector numSessionAppsVec;
    cOutVector numSinkAppsVec;
    cOutVector nodes;
    cOutVector matSrc; // record all the source servers of the created short flows
    cOutVector matDest; // record all the dest servers of the created short flows

    cMessage *startManagerNode;
    int IW;
    int switchQueueLength;
    const char *trafficMatrixType; // either "permTM"  or "randTM"
    TransportProtocol transportProtocol;
    bool perFlowEcmp;
    bool perPacketEcmp;

    int test = 0;
    int arrivalRate; // lamda of an exponential distribution (Morteza uses 256 and 2560)
    int flowSize;
    int numOfGroundStations;
    int numShortFlows;
    int longFlowSize;
    double percentLongFlowNodes;
    int numCompletedShortFlows = 0;
    int numCompletedLongFlows = 0;
    int numRunningShortFlowsNow = 0;
    /////????????????????
    cMessage *stopSimulation;
    std::vector<int> permServers;

    std::vector<int> permLongFlowsServers;
    std::vector<int> permShortFlowsServers;

    int numlongflowsRunningServers; // 33% of nodes run long flows
    int numshortflowRunningServers;

    struct coreAggMap
    {
        unsigned int aggIndex;
        std::vector<int> associatedCores; // associated

    };

    typedef std::list<coreAggMap> CoreAggMapList;
    CoreAggMapList coreAggMapList;

    std::vector<int> tempNode;
    std::vector<int> tempCombination;
    std::list<std::vector<int> > combinations; // all possible combinations
    unsigned int numAllCombinations;
    std::set<int> alreadySelectedGroups; // contains index of the selected groups (set.insert does not add in order like in vector.pushback)

    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override
    {
        Enter_Method_Silent
        ();
        throw cRuntimeError("Unsupported lifecycle operation '%s'", operation->getClassName());
        return true;
    }

    //  <dest, src>
    std::map<int, int> permMapLongFlows;
    std::map<int, int> permMapShortFlows;

    double sumArrivalTimes = 0;
    double newArrivalTime;
    bool shuffle = false;
    bool randomGroup = true;

    struct NodeLocation
    {
        unsigned int groundStationNumber;
        unsigned int numSink;
        unsigned int numSession;
    };

    typedef std::list<NodeLocation> NodeLocationList;
    NodeLocationList nodeLocationList;
    unsigned int seedValue;
    std::mt19937 PRNG;
    std::exponential_distribution<double> expDistribution;
    std::exponential_distribution<double> expDistributionForRqDecdoing;

    struct RecordMat
    {
        unsigned int recordSrc;
        unsigned int recordDest;
    };
    typedef std::list<RecordMat> RecordMatList;
    RecordMatList recordMatList;

    unsigned int numTimesDecodingFailed = 0;
    unsigned int numTimesDecodingSucceeded = 0;

    class Node {
    public:
        int row;
        int col;
        int dist;
        Node(int x, int y, int w)
            : row(x), col(y), dist(w)
        {
        }
    };

public:
    LeoFlowScheduler()
    {
    }
    virtual ~LeoFlowScheduler();
    double getNewValueFromExponentialDistribution();

protected:
    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;
    virtual void handleParameterChange(const char *parname) override;
    void serversLocations();
    void generateTM();

    void getNewDestRandTM(std::string &itsSrc, std::string &newDest);
    void getNewDestPremTM(std::string &itsSrc, std::string &newDest);

    void findLocation(unsigned int nodeIndex, std::string &nodePodRackLoc);
    void scheduleLongFlows();
    void deleteAllSubModuleApp(const char *subModuleToBeRemoved);
    int findNumSumbodules(cModule *nodeModule, const char *subModuleType);
    void scheduleNewShortFlow(std::string itsSrc, std::string newDest);

    void permTM(const char *longOrShortFlows);
    int getGroundStationNumber(unsigned int nodeIndex);
    int shortestPath(int src, int dest);
    void getWebSearchWorkLoad();
    unsigned int getNewFlowSizeFromWebSearchWorkLoad();
    unsigned int getPriorityValue(unsigned int flowSize);
    std::string getApplicationName(bool isClient);
    void setUpSrcModule(std::string itsSrc, std::string newDest, int newNumTpSinkAppsDest, bool isLongFlow);
    int setUpDestModule(std::string itsSrc, std::string newDest, bool isLongFlow);
};

}

#endif // ifndef __INET_RQ_H

