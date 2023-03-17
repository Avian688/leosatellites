#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "../networklayer/configurator/ipv4/LeoIpv4NetworkConfigurator.h"
#include <fstream>
#include <queue>

#include "LeoFlowScheduler.h"
using namespace std;
using namespace std::chrono;

namespace inet {

Define_Module(LeoFlowScheduler);
#define PKT_SIZE 1500
LeoFlowScheduler::~LeoFlowScheduler()
{
    cancelAndDelete(startManagerNode);
    cancelAndDelete(stopSimulation);

}

void LeoFlowScheduler::initialize(int stage)
{
    isWebSearchWorkLoad = par("isWebSearchWorkLoad");
    indexWorkLoad = 0;
    // Record start time
    t1 = high_resolution_clock::now();
    numCompletedShortFlows = par("numCompletedShortFlows");
    WATCH(numCompletedShortFlows);
    numCompletedLongFlows = par("numCompletedShortFlows");
    switchQueueLength = par("switchQueueLength");
    randMapShortFlowsVec.setName("randMapShortFlowsVec");
    permMapShortFlowsVector.setName("permMapShortFlowsVector");

    permMapLongFlowsVec.setName("permMapLongFlowsVec");
    permMapShortFlowsVec.setName("permMapShortFlowsVec");
    matSrc.setName("matSrc");
    matDest.setName("matDest");
    nodes.setName("nodes");

    std::cout << "\n\n Central flow scheduler \n";
    trafficMatrixType = par("trafficMatrixType");

    const char *tp = par("transportProtocol");
    const char *cApp = par("clientAppType");
    const char *rApp = par("receiverAppType");
    if(strcmp(tp, "tcp") == 0){transportProtocol = TCP;}
    else{EV_FATAL << "Not Valid Transport Protocol!";};

    arrivalRate = par("arrivalRate");

    perFlowEcmp = par("perFlowEcmp");
    perPacketEcmp = par("perPacketEcmp");

    // seed vale and rng
    seedValue = par("seedValue");
    srand(seedValue);
    PRNG = std::mt19937(seedValue);
    // note arrivalRate should be called before this line
    expDistribution = std::exponential_distribution<double>(arrivalRate);
    expDistributionForRqDecdoing = std::exponential_distribution<double>(2);

    flowSize = par("flowSize");
    numShortFlows = par("numShortFlows");
    longFlowSize = par("longFlowSize");
    numOfGroundStations = par("numOfGS");

    shuffle = par("shuffle");
    randomGroup = par("randomGroup");

    numRunningShortFlowsNow = par("numRunningShortFlowsNow");
    percentLongFlowNodes = par("percentLongFlowNodes");

    numTimesDecodingFailed = par("numTimesDecodingFailed");
    numTimesDecodingSucceeded = par("numTimesDecodingSucceeded");

    std::cout << " =====  SIMULATION CONFIGURATIONS ========= " << "\n";
    std::cout << " =====  ShortflowSize: " << flowSize << "      ========= \n";
    std::cout << " =====  numShortFlows: " << numShortFlows << "          ========= \n";
    std::cout << " =====  arrivalRate  : " << arrivalRate << "       ========= \n";
    std::cout << " ========================================== " << "\n";
    stopSimulation = new cMessage("stopSimulation");

    startManagerNode = new cMessage("startManagerNode");
    scheduleAt(0.001, startManagerNode);
}
void LeoFlowScheduler::handleMessage(cMessage *msg)
{
    if (msg == stopSimulation) {
        std::cout << " All shortFlows COMPLETED  " << std::endl;
        totalSimTime = simTime();
        endSimulation();
    }
    std::cout << "******************** LeoFlowScheduler::handleMessage .. ********************  \n";
    numlongflowsRunningServers = floor(numOfGroundStations * percentLongFlowNodes); // 33% of nodes run long flows , TODO throw error as it shouldn't be 1
    numshortflowRunningServers = numOfGroundStations - numlongflowsRunningServers;
    std::cout << "numshortflowRunningServers:  " << numshortflowRunningServers << std::endl;
    std::cout << "numlongflowsRunningServers:  " << numlongflowsRunningServers << std::endl;
    generateTM();
    serversLocations();
    std::string itsSrc;
    std::string newDest;

    deleteAllSubModuleApp("app[0]");
    std::cout << "\n\n ******************** schedule Long flows .. ********************  \n";
    scheduleLongFlows();
    std::cout << "\n\n ******************** schedule Short flows .. ********************  \n";
    if (isWebSearchWorkLoad == true)
        getWebSearchWorkLoad();
    for (unsigned int i = 1; i <= numShortFlows; i++) {
        std::cout << "\n\nShortflow ID: " << i << std::endl;
        if (strcmp(trafficMatrixType, "randTM") == 0)
            getNewDestRandTM(itsSrc, newDest);
        else if (strcmp(trafficMatrixType, "permTM") == 0)
            getNewDestPremTM(itsSrc, newDest);
        // identifying the servers locations: FatTree.Pod[].racks[].servers[]
        scheduleNewShortFlow(itsSrc, newDest);
    }
    std::cout << "\n\nCentral Scheduler complete!" << std::endl;
    std::cout << "\n\n\n";
}


void LeoFlowScheduler::serversLocations()
{
    std::cout << "\n\n ******************** serversLocations .. ********************  \n";

    for (int m = 0; m < numOfGroundStations; m++) {
        NodeLocation nodeLocation;
        nodeLocation.groundStationNumber = permServers.at(m);
        nodeLocation.numSink = 0;
        nodeLocation.numSession = 0;
        nodeLocationList.push_back(nodeLocation);
    }
    std::list<NodeLocation>::iterator it;
    it = nodeLocationList.begin();
    while (it != nodeLocationList.end()) {
        std::cout << " index: " << it->groundStationNumber;
        it++;
    }
}

// random TM
void LeoFlowScheduler::getNewDestRandTM(std::string &itsSrc, std::string &newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << "******************** getNewDestination RandTM .. ********************  \n";
    unsigned int newDestination = 0;
    unsigned int srcNewDestination = 0;

    while (newDestination == srcNewDestination) { // the dest should be different from the src
        newDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));
        //    int srcNewDestination = permMapShortFlows.find(newDestination)->second; // this line is used wit premTM not randTM
        srcNewDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));
    }
    std::cout << "@@@ newDestination " << newDestination << " , its src   " << srcNewDestination << "\n";

    findLocation(newDestination, newDest);
    findLocation(srcNewDestination, itsSrc);

    RecordMat recordMat;
    recordMat.recordSrc = srcNewDestination;
    recordMat.recordDest = newDestination;
    recordMatList.push_back(recordMat);

    // can be replaced by recordMatList ( see Finish())
    randMapShortFlowsVec.record(srcNewDestination);
    randMapShortFlowsVec.record(newDestination);
}

// permutation TM
void LeoFlowScheduler::generateTM()
{
    std::cout << "\n\n ******************** generate TM maps.. ********************  " << endl;
    for (int i = 0; i < numOfGroundStations; ++i){
        permServers.push_back(i);
    }
    if (shuffle){
        std::shuffle(permServers.begin(), permServers.end(), PRNG);
    }
    for (int i = 0; i < numOfGroundStations; ++i) {
        if (i < numlongflowsRunningServers) {
            permLongFlowsServers.push_back(permServers.at(i));
            permMapLongFlows.insert(std::pair<int, int>(permServers.at((i + 1) % numlongflowsRunningServers), permServers.at(i)));  // < dest, src >
        }
        else if (i >= numlongflowsRunningServers && i < numOfGroundStations - 1) {
            permShortFlowsServers.push_back(permServers.at(i));
            int permServer1 = permServers.at(i + 1);
            int permServer2 = permServers.at(i);
            permMapShortFlows.insert(std::pair<int, int>(permServer1, permServer2));  // < dest, src >
        }
        else if (i == numOfGroundStations - 1) {
//            permShortFlowsServers.push_back(permServers.at(numlongflowsRunningServers));
            permShortFlowsServers.push_back(permServers.at(i));
            permMapShortFlows.insert(std::pair<int, int>(permServers.at(numlongflowsRunningServers), permServers.at(i)));  // < dest, src >
        }
    }
    std::cout << "permServers:                ";
    for (std::vector<int>::iterator it = permServers.begin(); it != permServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    std::cout << "permLongFlowsServers:       ";
    for (std::vector<int>::iterator it = permLongFlowsServers.begin(); it != permLongFlowsServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    std::cout << "permShortFlowsServers:      ";
    for (std::vector<int>::iterator it = permShortFlowsServers.begin(); it != permShortFlowsServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    std::cout << "permMapLongFlows:                 \n";
    for (std::map<int, int>::iterator iter = permMapLongFlows.begin(); iter != permMapLongFlows.end(); ++iter) {
        cout << "  src " << iter->second << " ==> ";
        cout << "  dest " << iter->first << "\n";
        permMapLongFlowsVec.record(iter->second);
        permMapLongFlowsVec.record(iter->first);
    }

    std::cout << "permMapShortFlows:                 \n";
    for (std::map<int, int>::iterator iter = permMapShortFlows.begin(); iter != permMapShortFlows.end(); ++iter) {
        cout << "   src " << iter->second << " ==> ";
        cout << "   dest " << iter->first << "\n";
        permMapShortFlowsVec.record(iter->second);
        permMapShortFlowsVec.record(iter->first);
    }

}

int LeoFlowScheduler::shortestPath(int src, int dest)
{
    int gridSize = sqrt(numOfGroundStations);
    int grid[gridSize][gridSize];
    int val=0;
    for(int i = 0; i < gridSize; i++){
        for(int j = 0; j < gridSize ; j++)
        {
            grid[i][j]=val;
            val++;
        }
    }
    Node source(0, 0, 0);
    bool visited[gridSize][gridSize];
    for (int i = 0; i < gridSize; i++) {
        for (int j = 0; j < gridSize; j++)
        {
            if (grid[i][j] == '0')
                visited[i][j] = true;
            else
                visited[i][j] = false;

            // Finding source
            if (grid[i][j] == src)
            {
               source.row = i;
               source.col = j;
            }
        }
    }
    std::queue<Node> q;
    q.push(source);
    visited[source.row][source.col] = true;
    while (!q.empty()) {
        Node p = q.front();
        q.pop();
        if (grid[p.row][p.col] == dest)
            return p.dist + 2;
        if (p.row - 1 >= 0 &&
            visited[p.row - 1][p.col] == false) {
            q.push(Node(p.row - 1, p.col, p.dist + 1));
            visited[p.row - 1][p.col] = true;
        }
        if (p.row + 1 < gridSize &&
            visited[p.row + 1][p.col] == false) {
            q.push(Node(p.row + 1, p.col, p.dist + 1));
            visited[p.row + 1][p.col] = true;
        }
        if (p.col - 1 >= 0 &&
            visited[p.row][p.col - 1] == false) {
            q.push(Node(p.row, p.col - 1, p.dist + 1));
            visited[p.row][p.col - 1] = true;
        }
        if (p.col + 1 < gridSize &&
            visited[p.row][p.col + 1] == false) {
            q.push(Node(p.row, p.col + 1, p.dist + 1));
            visited[p.row][p.col + 1] = true;
        }
    }
    return -1;
}

void LeoFlowScheduler::getNewDestPremTM(std::string &itsSrc, std::string &newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << "******************** getNewDestination PremTM .. ********************  \n";
    int newDestination = permServers.at(numlongflowsRunningServers + (std::rand() % (numshortflowRunningServers)));

    int srcNewDestination = permMapShortFlows.find(newDestination)->second;
    std::cout << "@@@ newDestination " << newDestination << " , its src   " << srcNewDestination << "\n";

    findLocation(newDestination, newDest);
    findLocation(srcNewDestination, itsSrc);

    RecordMat recordMat;
    recordMat.recordSrc = srcNewDestination;
    recordMat.recordDest = newDestination;
    recordMatList.push_back(recordMat);

    permMapShortFlowsVector.record(srcNewDestination);
    permMapShortFlowsVector.record(newDestination);
}

int LeoFlowScheduler::getGroundStationNumber(unsigned int nodeIndex)
{
    std::list<NodeLocation>::iterator itt;
    itt = nodeLocationList.begin();
    while (itt != nodeLocationList.end()) {
        if (itt->groundStationNumber == nodeIndex) {
            return itt->groundStationNumber;
        }
        itt++;
    }
    return -1;
}
void LeoFlowScheduler::findLocation(unsigned int nodeIndex, std::string &nodeLoc)
{
    std::list<NodeLocation>::iterator itt;
    itt = nodeLocationList.begin();
    while (itt != nodeLocationList.end()) {
        if (itt->groundStationNumber == nodeIndex) {
            std::string networkName = this->getParentModule()->getFullName();
            nodeLoc = networkName +".groundStation[" + std::to_string(itt->groundStationNumber) + "]";
        }
        itt++;
    }
}

std::string LeoFlowScheduler::getApplicationName(bool isClient){
    switch(transportProtocol){
        case TCP:
            if(isClient){
                return "inet.applications.tcpapp.TcpSessionApp";
            }
            else{
                return "inet.applications.tcpapp.TcpSinkApp";
            }
        default:
            EV_FATAL << "Not Valid Transport Protocol!";
    }
    return "";
}

void LeoFlowScheduler::scheduleLongFlows()
{
    std::cout << "\n\n ******************** scheduleLongFlows .. ********************  \n";
    std::string dest;
    std::string source;

// iterate permMapLongFlows

    for (std::map<int, int>::iterator iter = permMapLongFlows.begin(); iter != permMapLongFlows.end(); ++iter) {
        cout << "\n\n NEW LONGFLOW :)   ";
        cout << "  host(SRC.)= " << iter->second << " ==> " << "  host(DEST.)= " << iter->first << "\n";

        RecordMat recordMat;
        recordMat.recordSrc = iter->second;
        recordMat.recordDest = iter->first;
        recordMatList.push_back(recordMat);

        LeoFlowScheduler::findLocation(iter->first, dest); // get dest value
        LeoFlowScheduler::findLocation(iter->second, source);
        cout << "  nodePodRackLoc:  " << iter->second << " == " << source << " ==> " << iter->first << " == " << dest << "\n";

        int numApps = setUpDestModule(source,dest, true);
        setUpSrcModule(source,dest, numApps, true);
    }
}
void LeoFlowScheduler::setUpSrcModule(std::string itsSrc, std::string newDest, int newNumTpSinkAppsDest, bool isLongFlow)
{
    cModule *srcModule = getModuleByPath(itsSrc.c_str());  // const char* c_str Return pointer to the string.
    cModule *tpSrcModule = srcModule->getSubmodule("at");
    unsigned int newTPGateOutSizeSrc = tpSrcModule->gateSize("out") + 1;
    unsigned int newTPGateInSizeSrc = tpSrcModule->gateSize("in") + 1;
    tpSrcModule->setGateSize("out", newTPGateOutSizeSrc);
    tpSrcModule->setGateSize("in", newTPGateInSizeSrc);
    unsigned int newNumTpSessionAppsSrc = findNumSumbodules(srcModule, getApplicationName(true).c_str()) + findNumSumbodules(srcModule, getApplicationName(false).c_str()) + 1;
    std::cout << "Src  numScdpSessionApp =  " << newNumTpSessionAppsSrc << "\n";
    cModuleType *moduleTypeSrc = cModuleType::get(getApplicationName(true).c_str());
    std::string nameTpAppSrc = "app[" + std::to_string(newNumTpSessionAppsSrc - 1) + "]";
    cModule *newSrcAppModule = moduleTypeSrc->create(nameTpAppSrc.c_str(), srcModule);
    newSrcAppModule->par("localAddress").setStringValue(itsSrc);
    newSrcAppModule->par("connectPort").setIntValue(80 + newNumTpSinkAppsDest);

    int sizeOfFlows = flowSize;
    if (isWebSearchWorkLoad == true) {
        sizeOfFlows = getNewFlowSizeFromWebSearchWorkLoad();
    }

    switch(transportProtocol){
        case TCP:
            {
            L3Address destination;
            L3AddressResolver().tryResolve(newDest.c_str(), destination);
            newSrcAppModule->par("connectAddress").setStringValue(destination.str());
            newSrcAppModule->par("tOpen").setDoubleValue(simTime().dbl() + sumArrivalTimes);
            newSrcAppModule->par("tSend").setDoubleValue(simTime().dbl() +  sumArrivalTimes);
            if(isLongFlow){
                newSrcAppModule->par("sendBytes").setIntValue(longFlowSize*1000);
            }
            else{
                newSrcAppModule->par("sendBytes").setIntValue(sizeOfFlows*1000);
            }
            break;
            }
    }
    cGate *gateTpIn = tpSrcModule->gate("in", newTPGateInSizeSrc - 1);
    cGate *gateTpOut = tpSrcModule->gate("out", newTPGateOutSizeSrc - 1);
    cGate *gateIn = newSrcAppModule->gate("socketIn");
    cGate *gateOut = newSrcAppModule->gate("socketOut");
    gateTpOut->connectTo(gateIn);
    gateOut->connectTo(gateTpIn);
    newSrcAppModule->finalizeParameters();
    newSrcAppModule->buildInside();
    newSrcAppModule->scheduleStart(simTime());
    newSrcAppModule->callInitialize();
}

int LeoFlowScheduler::setUpDestModule(std::string itsSrc, std::string newDest, bool isLongFlow)
{
    //std::cout << "srcModule:  " << srcModule->getFullPath() << "  , destModule:  " << destModule->getFullPath() << std::endl << std::endl;
    cModule *destModule = getModuleByPath(newDest.c_str());
    cModule *tpDestModule = destModule->getSubmodule("at");
    unsigned int newTPGateOutSizeDest = tpDestModule->gateSize("out") + 1;
    unsigned int newTPGateInSizeDest = tpDestModule->gateSize("in") + 1;
    tpDestModule->setGateSize("out", newTPGateOutSizeDest);
    tpDestModule->setGateSize("in", newTPGateInSizeDest);
    unsigned int newNumTpSinkAppsDest = findNumSumbodules(destModule, getApplicationName(false).c_str()) + findNumSumbodules(destModule, getApplicationName(true).c_str()) + 1;
    std::cout << "Dest  NumSinkApp   =  " << newNumTpSinkAppsDest << "\n";
    cModuleType *moduleTypeDest = cModuleType::get(getApplicationName(false).c_str());
    std::string nameTpAppDest = "app[" + std::to_string(newNumTpSinkAppsDest - 1) + "]";
    cModule *newDestAppModule = moduleTypeDest->create(nameTpAppDest.c_str(), destModule);
    newDestAppModule->par("localPort").setIntValue(80 + newNumTpSinkAppsDest);
    if(transportProtocol == TCP){
           L3Address destination;
           L3AddressResolver().tryResolve(newDest.c_str(), destination);
           newDestAppModule->par("localAddress").setStringValue(destination.str());
    }
    cGate *gateTpInDest = tpDestModule->gate("in", newTPGateOutSizeDest - 1);
    cGate *gateTpOutDest = tpDestModule->gate("out", newTPGateOutSizeDest - 1);
    cGate *gateInDest = newDestAppModule->gate("socketIn");
    cGate *gateOutDest = newDestAppModule->gate("socketOut");
    gateTpOutDest->connectTo(gateInDest);
    gateOutDest->connectTo(gateTpInDest);
    newDestAppModule->finalizeParameters();
    newDestAppModule->buildInside();
    newDestAppModule->scheduleStart(simTime());
    newDestAppModule->callInitialize();
    return newNumTpSinkAppsDest;
}

void LeoFlowScheduler::scheduleNewShortFlow(std::string itsSrc, std::string newDest)
{
    std::cout << "@@@@@@@@@@@@@@@@@@@@@@@@@ scheduleNewShortFlow .. @@@@@@@@@@@@@@@@@@@@@@@@@  \n";
    std::cout << " newDest " << newDest << "\n";
    std::cout << " itsSrc " << itsSrc << "\n";

    newArrivalTime = expDistribution.operator()(PRNG);
    sumArrivalTimes = sumArrivalTimes + newArrivalTime;
    int numApps = setUpDestModule(itsSrc,newDest, false);
    setUpSrcModule(itsSrc,newDest, numApps, false);
}



int LeoFlowScheduler::findNumSumbodules(cModule *nodeModule, const char *subModuleType)
{
    int rep = 0;
    for (cModule::SubmoduleIterator iter(nodeModule); !iter.end(); iter++) {
        cModule *subModule = *iter;
        if (strcmp(subModule->getModuleType()->getFullName(), subModuleType) == 0) {
            rep++;
        }
    }
    return rep;
}

void LeoFlowScheduler::deleteAllSubModuleApp(const char *subModuleToBeRemoved)
{
    std::cout << "\n\n ******************** deleteAll temp SubModuleApp  .. ********************  \n";
    std::string node;
    for (int i = 0; i < numOfGroundStations; i++) {
        LeoFlowScheduler::findLocation(i, node);
        cModule *nodeModule = getModuleByPath(node.c_str());
        cModule *tempTpAppModule = nullptr;
        for (cModule::SubmoduleIterator iter(nodeModule); !iter.end(); iter++) {
            cModule *subModule = *iter;
            if (strcmp(subModule->getFullName(), subModuleToBeRemoved) == 0) {
                tempTpAppModule = subModule;
            }
        }
        std::cout << "Deleting " << tempTpAppModule->str() << endl;
        tempTpAppModule->deleteModule();
    }
    std::cout << " Done.. \n";

}

// permutation Traffic Matrix
void LeoFlowScheduler::permTM(const char *longOrShortFlows)
{
}

double LeoFlowScheduler::getNewValueFromExponentialDistribution()
{
    return expDistributionForRqDecdoing.operator()(PRNG);
}

void LeoFlowScheduler::finish()
{

    for (std::vector<int>::iterator iter = permServers.begin(); iter != permServers.end(); ++iter) {
        cout << "  NODE= " << *iter << "  ";
        nodes.record(*iter);

        std::string source;
        LeoFlowScheduler::findLocation(*iter, source); // get dest value
        cModule *srcModule = getModuleByPath(source.c_str());

        int finalNumSessionApps = findNumSumbodules(srcModule, "inet.raptorqimplementation.application.raptorqapp.RaptorQBasicClientApp");
        int finalNumSinkApps = findNumSumbodules(srcModule, "inet.raptorqimplementation.application.raptorqapp.RQSinkApp");

        std::cout << "  finalNumTcpSessionApps:  " << finalNumSessionApps << ",  finalNumTcpSinkApps: " << finalNumSinkApps << "\n";
        numSessionAppsVec.record(finalNumSessionApps);
        numSinkAppsVec.record(finalNumSinkApps);
    }

    std::cout << "numshortflowRunningServers:  " << numshortflowRunningServers << std::endl;
    std::cout << "numlongflowsRunningServers:  " << numlongflowsRunningServers << std::endl;
    std::cout << "test reach" << endl;
    std::cout << "permLongFlowsServers:       ";
    for (std::vector<int>::iterator it = permLongFlowsServers.begin(); it != permLongFlowsServers.end(); ++it)
        std::cout << ' ' << *it;
    std::cout << '\n';

    // Record end time
    t2 = high_resolution_clock::now();
    short duration = std::chrono::duration_cast<minutes>(t2 - t1).count();
    std::cout << "=================================================== " << std::endl;
    std::cout << " total Wall Clock Time (Real Time) = " << duration << " minutes" << std::endl;
    std::cout << " total Simulation Time      = " << totalSimTime << " sec" << std::endl;
    std::cout << "=================================================== " << std::endl;
    std::cout << "-------------------------------------------------------- " << std::endl;
    std::cout << " num completed shortflows = " << numCompletedShortFlows << std::endl;
    recordScalar("simTimeTotal=", totalSimTime);
    recordScalar("numShortFlows=", numShortFlows);
    recordScalar("flowSize=", flowSize);
    recordScalar("percentLongFlowNodes=", percentLongFlowNodes);
    recordScalar("arrivalRate=", arrivalRate);
    if (strcmp(trafficMatrixType, "permTM") == 0)
        recordScalar("permTM", 1);
    if (strcmp(trafficMatrixType, "randTM") == 0)
        recordScalar("randTM", 1);
    recordScalar("wallClockTime=", duration);
    recordScalar("switchQueueLength=", switchQueueLength);

    recordScalar("perFlowEcmp=", perFlowEcmp);
    recordScalar("perPacketEcmp=", perPacketEcmp);
    recordScalar("seedValue=", seedValue);
    recordScalar("isWebSearchWorkLoad=", isWebSearchWorkLoad);

    unsigned int numDecodingWasntNeeded = numCompletedShortFlows - numTimesDecodingSucceeded;        // >> NEW

    recordScalar("numDecodingWasntNeeded=", numDecodingWasntNeeded);        // >> NEW
    recordScalar("numDecodingFailed=", numTimesDecodingFailed); // >> NEW

    recordScalar("numTimesDecodingSucceeded=", numTimesDecodingSucceeded); // >> NEW
    std::cout << " # decoding    succeeded  =  " << numTimesDecodingSucceeded << "\n";
    std::cout << " # decoding     failed    =  " << numTimesDecodingFailed << "\n";
    std::cout << " # decoding wasn't needed =  " << numDecodingWasntNeeded << "\n"; // >> NEW
    std::cout << "-------------------------------------------------------- " << std::endl;
    std::list<RecordMat>::iterator itt;
    itt = recordMatList.begin();
    while (itt != recordMatList.end()) {
        matSrc.record(itt->recordSrc);
        matDest.record(itt->recordDest);
        itt++;
    }
}

void LeoFlowScheduler::handleParameterChange(const char *parname)
{
    if (parname && strcmp(parname, "numCompletedShortFlows") == 0) {
        --numRunningShortFlowsNow;
        ++numCompletedShortFlows;
        std::cout << "======================================  " << "\n";
        std::cout << " num completed shortflows =  " << numCompletedShortFlows << "\n";
        std::cout << "======================================  " << "\n\n\n";

        if (numCompletedShortFlows == numShortFlows) {
            scheduleAt(simTime(), stopSimulation);
        }
    }

    if (parname && strcmp(parname, "numCompletedLongFlows") == 0) {
        ++numCompletedLongFlows;
    }

    if (parname && strcmp(parname, "numRunningShortFlowsNow") == 0) {
        ++numRunningShortFlowsNow;
        std::cout << " numRunningShortFlowsNow r....r..r....r....r....r...: " << numRunningShortFlowsNow << "\n\n\n";
    }

    if (parname && strcmp(parname, "numTimesDecodingSucceeded") == 0) {
        ++numTimesDecodingSucceeded;
    }

    if (parname && strcmp(parname, "numTimesDecodingFailed") == 0) {
        ++numTimesDecodingFailed;
    }

}

void LeoFlowScheduler::getWebSearchWorkLoad()
{
    unsigned int numFlows = numShortFlows;
    double a[numFlows];

    std::ifstream myfile("../inputWokLoad.txt");
    if (myfile.is_open()) {
        int i = 0;
        while (i < numFlows && myfile >> a[i]) {
            flowSizeWebSeachWorkLoad.push_back(a[i]);
            i++;
        }
        myfile.close();
    }
    else
        std::cerr << "Unable to open file" << endl;
}

unsigned int LeoFlowScheduler::getNewFlowSizeFromWebSearchWorkLoad()
{
    unsigned int newFLowSize = flowSizeWebSeachWorkLoad.at(indexWorkLoad);
    ++indexWorkLoad;
    return newFLowSize;
}

// auto pfabric pias homa ndp phost

// 0     --> 10KB    P=1
// 10KB  --> 100KB   P=2
// 100KB --> 1MB     P=3
// 1MB   --> 10MB    P=4
// otherwise (longflows)         P=0 (RaptorQBasicClientApp.ned --> int priorityValue = default(0);)
unsigned int LeoFlowScheduler::getPriorityValue(unsigned int flowSize)
{
    unsigned int priorityValue;
    if (flowSize >= 1 && flowSize <= 7) {
        priorityValue = 1;
        return priorityValue;
    }

    if (flowSize >= 8 && flowSize <= 67) {
        priorityValue = 2;
        return priorityValue;
    }

    if (flowSize >= 68 && flowSize <= 667) {
        priorityValue = 3;
        return priorityValue;
    }

    if (flowSize >= 668 && flowSize <= 6667) {
        priorityValue = 4;
        return priorityValue;
    }

    return 0;
}

} //namespace inet
