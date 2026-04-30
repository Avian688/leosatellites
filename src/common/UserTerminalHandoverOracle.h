#ifndef COMMON_USERTERMINALHANDOVERORACLE_H_
#define COMMON_USERTERMINALHANDOVERORACLE_H_

#include <inet/common/INETDefs.h>

#include <unordered_map>
#include <vector>

namespace inet {

class INET_API UserTerminalHandoverOracle : public cSimpleModule {
  protected:
    std::vector<cModule *> userTerminalModules;
    std::unordered_map<cModule *, int> userTerminalIndexByModule;
    std::vector<bool> hardHandoverActive;
    std::vector<simtime_t> handoverStartTimes;
    std::vector<simtime_t> nextHandoverStartTimes;
    std::vector<simtime_t> expectedHandoverEndTimes;
    std::vector<simtime_t> lastHandoverEndTimes;
    std::vector<int> oldSatelliteIndexes;
    std::vector<int> newSatelliteIndexes;

    virtual void initialize(int stage) override;
    void resizeState(int userTerminalCount);
    bool isValidUserTerminalIndex(int userTerminalIndex) const;

  public:
    int getNumUserTerminals() const;
    cModule *getUserTerminalModule(int userTerminalIndex) const;
    int getUserTerminalIndex(cModule *userTerminalModule) const;

    bool isHardHandoverActive(int userTerminalIndex) const;
    bool isHardHandoverActive(cModule *userTerminalModule) const;
    bool getHardHandoverActive(int userTerminalIndex) const;
    bool hasAnyHardHandoverActive() const;

    simtime_t getHandoverStartTime(int userTerminalIndex) const;
    simtime_t getNextHandoverStartTime(int userTerminalIndex) const;
    simtime_t getExpectedHandoverEndTime(int userTerminalIndex) const;
    simtime_t getLastHandoverEndTime(int userTerminalIndex) const;
    int getOldSatelliteIndex(int userTerminalIndex) const;
    int getNewSatelliteIndex(int userTerminalIndex) const;

    void setUserTerminals(const std::vector<cModule *>& userTerminals);
    void setNextHandoverStartTime(int userTerminalIndex, simtime_t nextStartTime);
    void setNextHandoverStartTimes(simtime_t nextStartTime);
    void markHardHandoverStarted(int userTerminalIndex, int oldSatelliteIndex, int newSatelliteIndex,
                                 simtime_t startTime, simtime_t expectedEndTime);
    void markHardHandoverEnded(int userTerminalIndex, simtime_t endTime);
};

} // namespace inet

#endif /* COMMON_USERTERMINALHANDOVERORACLE_H_ */
