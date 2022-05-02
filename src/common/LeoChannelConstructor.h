#ifndef COMMON_LEOCHANNELCONSTRUCTOR_H_
#define COMMON_LEOCHANNELCONSTRUCTOR_H_

#include <inet/common/INETDefs.h>
#include <inet/common/ModuleAccess.h>
#include <inet/networklayer/common/L3AddressResolver.h>
#include <inet/common/lifecycle/ILifecycle.h>
#include <inet/networklayer/common/InterfaceTable.h>
#include <inet/networklayer/common/InterfaceEntry.h>
#include <inet/linklayer/ppp/Ppp.h>

#include "../networklayer/configurator/ipv4/SatelliteNodeConfigurator.h"
#include "../networklayer/configurator/ipv4/LeoNetworkConfigurator.h"
#include "../mobility/SatelliteMobility.h"
#include "../mobility/GroundStationMobility.h"

namespace inet{

class INET_API LeoChannelConstructor: public cSimpleModule, public ILifecycle {
public:
    LeoChannelConstructor();
    virtual ~LeoChannelConstructor();

protected:
    LeoNetworkConfigurator* configurator;

    cMessage *startManagerNode;
    cMessage *updateTimer;
    simtime_t updateInterval;

    virtual bool handleOperationStage(LifecycleOperation *operation, IDoneCallback *doneCallback) override;
    void updatePPPModules(cModule *mod);
    std::pair<cGate*,cGate*> getNextFreeGate(cModule *mod);
    void setUpInterfaces();

    virtual void initialize(int stage) override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void setUpSimulation();
    virtual void updateChannels();
    virtual void setUpGSLinks();
    virtual void addPPPInterfaces();
    virtual void prepareInterface(InterfaceEntry *interfaceEntry);
    void scheduleUpdate();
    virtual void finish() override;

protected:
    std::string networkName;
    int numOfSats;
    int numOfGS;
    int numOfPlanes;
    int satPerPlane;

    Ipv4Address addressBase;
    Ipv4Address netmask;
};

}

#endif /* COMMON_LEOCHANNELCONSTRUCTOR_H_ */
