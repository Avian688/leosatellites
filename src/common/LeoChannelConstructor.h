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

#include "../networklayer/ipv4/LeoIpv4RoutingTable.h"
#include "../networklayer/configurator/ipv4/LeoIpv4NetworkConfigurator.h"
#include "../mobility/SatelliteMobility.h"
#include "../mobility/GroundStationMobility.h"

namespace inet{

class INET_API LeoChannelConstructor: public cSimpleModule, public LifecycleController {
public:
    LeoChannelConstructor();
    virtual ~LeoChannelConstructor();

protected:
    LeoIpv4NetworkConfigurator* configurator;

    cMessage *startManagerNode;
    cMessage *updateTimer;
    simtime_t updateInterval;
    std::string linkDataRate;
    int queueSize;
    simtime_t currentInterval;

    void updatePPPModules(cModule *mod, bool addToGraph);
    void setUpIpLayer(cModule* module, NetworkInterface* interface);
    std::pair<cGate*,cGate*> getNextFreeGate(cModule *mod);
    void setUpInterfaces();

    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void setUpSimulation();
    virtual void updateChannels();
    virtual void setUpGSLinks();
    virtual void setUpClientServerInterfaces();
    virtual void addPPPInterfaces();
    virtual void prepareInterface(NetworkInterface *interfaceEntry);
    void createChannel(std::string delay, cGate *gate1, cGate*gate2, bool endPointChannel);
    void scheduleUpdate(bool simStart);
    void processLifecycleCommand(cModule *module, std::string command);
    virtual void finish() override;

protected:
    std::string networkName;
    int numOfSats;
    int numOfGS;
    int numOfPlanes;
    int numOfClients;
    int satPerPlane;
    const char* interfaceType;

    Ipv4Address addressBase;
    Ipv4Address netmask;

    bool enableInterSatelliteLinks;
};

}

#endif /* COMMON_LEOCHANNELCONSTRUCTOR_H_ */
