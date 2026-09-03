//
// Ping application that selects a saved endpoint K-shortest path.
//

#ifndef APPLICATIONS_PINGAPP_KSHORTESTPATHPINGAPP_H_
#define APPLICATIONS_PINGAPP_KSHORTESTPATHPINGAPP_H_

#include <inet/applications/pingapp/PingApp.h>

namespace inet {

class LeoIpv4NetworkConfigurator;

class INET_API KShortestPathPingApp : public PingApp
{
  protected:
    int pathGroup = -1;
    int pathIndex = -1;
    LeoIpv4NetworkConfigurator *configurator = nullptr;

    static simsignal_t pathAvailableSignal;
    static simsignal_t expectedRttSignal;
    static simsignal_t coreLinkCountSignal;
    static simsignal_t catalogSizeSignal;

    virtual void initialize(int stage) override;
    virtual void startSendingPingRequests() override;
    virtual void sendPingRequest() override;
    void emitPathState();
};

} // namespace inet

#endif
