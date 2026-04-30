#include "UserTerminalHandoverOracle.h"

namespace inet {

Define_Module(UserTerminalHandoverOracle);

void UserTerminalHandoverOracle::initialize(int stage)
{
    if (stage != INITSTAGE_LOCAL)
        return;

    cModule *parent = getParentModule();
    const int userTerminalCount = parent != nullptr && parent->hasPar("numOfUserTerminals") ? parent->par("numOfUserTerminals").intValue() : 0;

    std::vector<cModule *> userTerminals(userTerminalCount, nullptr);
    if (parent != nullptr) {
        for (int userTerminalIndex = 0; userTerminalIndex < userTerminalCount; userTerminalIndex++)
            userTerminals[userTerminalIndex] = parent->getSubmodule("userTerminal", userTerminalIndex);
    }

    setUserTerminals(userTerminals);
}

void UserTerminalHandoverOracle::resizeState(int userTerminalCount)
{
    userTerminalModules.resize(userTerminalCount, nullptr);
    hardHandoverActive.assign(userTerminalCount, false);
    handoverStartTimes.assign(userTerminalCount, SIMTIME_ZERO);
    nextHandoverStartTimes.assign(userTerminalCount, SIMTIME_ZERO);
    expectedHandoverEndTimes.assign(userTerminalCount, SIMTIME_ZERO);
    lastHandoverEndTimes.assign(userTerminalCount, SIMTIME_ZERO);
    oldSatelliteIndexes.assign(userTerminalCount, -1);
    newSatelliteIndexes.assign(userTerminalCount, -1);
}

bool UserTerminalHandoverOracle::isValidUserTerminalIndex(int userTerminalIndex) const
{
    return userTerminalIndex >= 0 && userTerminalIndex < static_cast<int>(hardHandoverActive.size());
}

int UserTerminalHandoverOracle::getNumUserTerminals() const
{
    return userTerminalModules.size();
}

cModule *UserTerminalHandoverOracle::getUserTerminalModule(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? userTerminalModules[userTerminalIndex] : nullptr;
}

int UserTerminalHandoverOracle::getUserTerminalIndex(cModule *userTerminalModule) const
{
    auto it = userTerminalIndexByModule.find(userTerminalModule);
    return it == userTerminalIndexByModule.end() ? -1 : it->second;
}

bool UserTerminalHandoverOracle::isHardHandoverActive(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) && hardHandoverActive[userTerminalIndex];
}

bool UserTerminalHandoverOracle::isHardHandoverActive(cModule *userTerminalModule) const
{
    return isHardHandoverActive(getUserTerminalIndex(userTerminalModule));
}

bool UserTerminalHandoverOracle::getHardHandoverActive(int userTerminalIndex) const
{
    return isHardHandoverActive(userTerminalIndex);
}

bool UserTerminalHandoverOracle::hasAnyHardHandoverActive() const
{
    for (bool active : hardHandoverActive) {
        if (active)
            return true;
    }
    return false;
}

simtime_t UserTerminalHandoverOracle::getHandoverStartTime(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? handoverStartTimes[userTerminalIndex] : SIMTIME_ZERO;
}

simtime_t UserTerminalHandoverOracle::getNextHandoverStartTime(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? nextHandoverStartTimes[userTerminalIndex] : SIMTIME_ZERO;
}

simtime_t UserTerminalHandoverOracle::getExpectedHandoverEndTime(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? expectedHandoverEndTimes[userTerminalIndex] : SIMTIME_ZERO;
}

simtime_t UserTerminalHandoverOracle::getLastHandoverEndTime(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? lastHandoverEndTimes[userTerminalIndex] : SIMTIME_ZERO;
}

int UserTerminalHandoverOracle::getOldSatelliteIndex(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? oldSatelliteIndexes[userTerminalIndex] : -1;
}

int UserTerminalHandoverOracle::getNewSatelliteIndex(int userTerminalIndex) const
{
    return isValidUserTerminalIndex(userTerminalIndex) ? newSatelliteIndexes[userTerminalIndex] : -1;
}

void UserTerminalHandoverOracle::setUserTerminals(const std::vector<cModule *>& userTerminals)
{
    resizeState(userTerminals.size());
    userTerminalIndexByModule.clear();

    for (int userTerminalIndex = 0; userTerminalIndex < static_cast<int>(userTerminals.size()); userTerminalIndex++) {
        cModule *userTerminalModule = userTerminals[userTerminalIndex];
        userTerminalModules[userTerminalIndex] = userTerminalModule;
        if (userTerminalModule != nullptr)
            userTerminalIndexByModule[userTerminalModule] = userTerminalIndex;
    }
}

void UserTerminalHandoverOracle::setNextHandoverStartTime(int userTerminalIndex, simtime_t nextStartTime)
{
    if (!isValidUserTerminalIndex(userTerminalIndex))
        return;

    nextHandoverStartTimes[userTerminalIndex] = nextStartTime;
}

void UserTerminalHandoverOracle::setNextHandoverStartTimes(simtime_t nextStartTime)
{
    for (int userTerminalIndex = 0; userTerminalIndex < static_cast<int>(nextHandoverStartTimes.size()); userTerminalIndex++)
        nextHandoverStartTimes[userTerminalIndex] = nextStartTime;
}

void UserTerminalHandoverOracle::markHardHandoverStarted(int userTerminalIndex, int oldSatelliteIndex, int newSatelliteIndex,
                                                         simtime_t startTime, simtime_t expectedEndTime)
{
    if (!isValidUserTerminalIndex(userTerminalIndex))
        return;

    hardHandoverActive[userTerminalIndex] = true;
    handoverStartTimes[userTerminalIndex] = startTime;
    expectedHandoverEndTimes[userTerminalIndex] = expectedEndTime;
    oldSatelliteIndexes[userTerminalIndex] = oldSatelliteIndex;
    newSatelliteIndexes[userTerminalIndex] = newSatelliteIndex;
}

void UserTerminalHandoverOracle::markHardHandoverEnded(int userTerminalIndex, simtime_t endTime)
{
    if (!isValidUserTerminalIndex(userTerminalIndex))
        return;

    hardHandoverActive[userTerminalIndex] = false;
    lastHandoverEndTimes[userTerminalIndex] = endTime;
}

} // namespace inet
