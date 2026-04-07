#ifndef COMMON_LEOCHANNELCONSTRUCTOR_H_
#define COMMON_LEOCHANNELCONSTRUCTOR_H_

#include <inet/common/INETDefs.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/common/lifecycle/ILifecycle.h>
#include <inet/networklayer/common/InterfaceTable.h>
#include <inet/networklayer/common/NetworkInterface.h>
#include <inet/linklayer/ppp/Ppp.h>
#include <inet/networklayer/ipv4/Ipv4InterfaceData.h>
#include <inet/queueing/queue/PacketQueue.h>
#include <inet/common/lifecycle/LifecycleController.h>
#include <omnetpp/cdataratechannel.h>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../networklayer/ipv4/LeoIpv4RoutingTable.h"
#include "../networklayer/configurator/ipv4/LeoIpv4NetworkConfigurator.h"
#include "../mobility/SatelliteMobility.h"
#include "../mobility/GroundStationMobility.h"
#include "../mobility/INorad.h"

namespace inet{

class INET_API LeoChannelConstructor: public cSimpleModule, public LifecycleController {
public:
    LeoChannelConstructor();
    virtual ~LeoChannelConstructor();

protected:
    struct TimedLinkRecord {
        SatelliteMobility *sourceSatelliteMobility = nullptr;
        SatelliteMobility *destinationSatelliteMobility = nullptr;
        GroundStationMobility *destinationGroundStationMobility = nullptr;
        cDatarateChannel *forwardChannel = nullptr;
        cDatarateChannel *reverseChannel = nullptr;
        int satelliteIndex = -1;
        int groundStationIndex = -1;
        bool isGroundStationLink = false;
    };

    struct ActiveGroundStationLink {
        cGate *satelliteOutputGate = nullptr;
        cGate *groundStationOutputGate = nullptr;
        TimedLinkRecord linkRecord;
    };

    struct ActiveUserTerminalLink {
        cGate *satelliteOutputGate = nullptr;
        cGate *userTerminalOutputGate = nullptr;
        SatelliteMobility *satelliteMobility = nullptr;
        GroundStationMobility *userTerminalMobility = nullptr;
        cDatarateChannel *forwardChannel = nullptr;
        cDatarateChannel *reverseChannel = nullptr;
        int satelliteIndex = -1;
        int userTerminalIndex = -1;
    };

    struct PendingUserTerminalHandover {
        int userTerminalIndex = -1;
        int targetSatelliteIndex = -1;
        simtime_t reconnectTime = SIMTIME_ZERO;
    };

    struct TopocentricSample {
        double azimuth = 0;
        double elevation = 0;
        bool reachable = false;
    };

    LeoIpv4NetworkConfigurator* configurator;

    cMessage *startManagerNode;
    cMessage *updateTimer;
    cMessage *userTerminalHandoverTimer;
    simtime_t updateInterval;
    simtime_t userTerminalUpdateInterval;
    simtime_t userTerminalSampleInterval;
    simtime_t userTerminalHandoverDowntime;
    simtime_t nextUserTerminalUpdate;
    double linkDataRate = 0;
    int queueSize;
    simtime_t currentInterval;

    std::vector<cModule *> satelliteModules;
    std::vector<cModule *> groundStationModules;
    std::vector<cModule *> clientModules;
    std::vector<cModule *> serverModules;
    std::vector<cModule *> userTerminalModules;
    std::vector<SatelliteMobility *> satelliteMobilities;
    std::vector<INorad *> satelliteNoradModules;
    std::vector<GroundStationMobility *> groundStationMobilities;
    std::vector<GroundStationMobility *> userTerminalMobilities;
    std::vector<TimedLinkRecord> fixedTimedLinks;
    std::unordered_map<uint64_t, ActiveGroundStationLink> activeGroundStationLinks;
    std::unordered_map<int, ActiveUserTerminalLink> activeUserTerminalLinks;
    std::unordered_map<int, PendingUserTerminalHandover> pendingUserTerminalHandovers;
    std::unordered_set<cModule *> dirtyPppModules;

    void updatePPPModules(cModule *mod, bool addToGraph);
    void setUpIpLayer(cModule* module, NetworkInterface* interface);
    std::pair<cGate*,cGate*> getNextFreeGate(cModule *mod);
    void setUpInterfaces();
    void cacheNetworkModules();
    void updateTimedLink(const TimedLinkRecord& linkRecord);
    void createGroundStationLink(int gsNum, int satNum);
    void removeGroundStationLink(int gsNum, int satNum, ActiveGroundStationLink& link);
    bool refreshUserTerminalLinks(bool forceUpdate);
    int selectServingSatelliteForUserTerminal(int userTerminalIndex);
    void createUserTerminalLink(int userTerminalIndex, int satNum);
    void removeUserTerminalLink(int userTerminalIndex, ActiveUserTerminalLink& link);
    void finalizeUserTerminalLinks();
    bool completePendingUserTerminalHandovers();
    void scheduleUserTerminalHandoverTimer();
    std::vector<TopocentricSample> buildReferenceTrajectory(const GroundStationMobility *mobility, simtime_t slotStart, simtime_t slotEnd) const;
    std::vector<TopocentricSample> buildCandidateTrajectory(int satNum, const GroundStationMobility *mobility, simtime_t slotStart, simtime_t slotEnd) const;
    double computeDtwDistance(const std::vector<TopocentricSample>& referenceTrajectory, const std::vector<TopocentricSample>& candidateTrajectory) const;
    uint64_t getGroundStationLinkKey(int gsNum, int satNum) const;

    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void setUpSimulation();
    virtual void updateChannels();
    virtual void setUpGSLinks();
    virtual void setUpClientServerInterfaces();
    virtual void addPPPInterfaces();
    virtual void prepareInterface(NetworkInterface *interfaceEntry);
    void createChannel(double delaySeconds, cGate *gate1, cGate*gate2, bool endPointChannel);
    void scheduleUpdate(bool simStart);
    void processLifecycleCommand(cModule *module, std::string command);
    virtual void finish() override;

protected:
    std::string networkName;
    int numOfSats;
    int numOfGS;
    int numOfPlanes;
    int numOfClients;
    int numOfUserTerminals;
    int satPerPlane;
    const char* interfaceType;

    Ipv4Address addressBase;
    Ipv4Address netmask;

    bool enableInterSatelliteLinks;
};

}

#endif /* COMMON_LEOCHANNELCONSTRUCTOR_H_ */
