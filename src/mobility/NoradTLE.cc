#include "NoradTLE.h"

#include <ctime>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "libnorad/cTLE.h"
#include "libnorad/cOrbit.h"
#include "libnorad/cSite.h"

using namespace omnetpp;
Define_Module(NoradTLE);

namespace {

const std::vector<std::string>& getCachedTleLines(const std::string& filename)
{
    static std::unordered_map<std::string, std::vector<std::string>> tleCache;

    auto it = tleCache.find(filename);
    if (it != tleCache.end())
        return it->second;

    std::ifstream tleFile(filename.c_str());
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(tleFile, line))
        lines.push_back(line);

    return tleCache.emplace(filename, std::move(lines)).first->second;
}

}

NoradTLE::NoradTLE()
{
    gap = 0.0;
    tle = nullptr;
    orbit = nullptr;
}

void NoradTLE::finish()
{
    delete orbit;
    delete tle;
}

void NoradTLE::initializeMobility(const simtime_t& targetTime)
{
    std::string filename = par("TLEfile").stringValue();
    const auto& tleLines = getCachedTleLines(filename);

    std::string satelliteName = getParentModule()->par("satelliteName").stringValue();
    std::string line_str;
    std::string line1tmp;
    std::string line2tmp;
    if (satelliteName == "") {
        int index = getParentModule()->getIndex();
        const int baseLine = index * 3;
        if (baseLine + 2 >= static_cast<int>(tleLines.size())) {
            EV << "Error in Norad::initializeMobility(): Cannot read further satellites from TLE file!" << std::endl;
            endSimulation();
        }
        line_str = tleLines[baseLine];
        line1tmp = tleLines[baseLine + 1];
        line2tmp = tleLines[baseLine + 2];
    } else {
        size_t matchedLine = std::string::npos;
        for (size_t i = 0; i < tleLines.size(); i++) {
            if (tleLines[i].find(satelliteName.c_str()) != std::string::npos) {
                matchedLine = i;
                break;
            }
        }
        if (matchedLine == std::string::npos || matchedLine + 2 >= tleLines.size()) {
            EV << "Error in Norad::initializeMobility(): Unable to locate satellite in TLE file!" << std::endl;
            endSimulation();
        }
        line_str = tleLines[matchedLine];
        line1tmp = tleLines[matchedLine + 1];
        line2tmp = tleLines[matchedLine + 2];
    }

    // Pretty up the satellites name
    line_str = line_str.substr(0, line_str.find("  "));
    line0 = line_str;
    line1 = line1tmp;
    line2 = line2tmp;
    cTle tle(line0, line1, line2);
    orbit = new cOrbit(tle);

    raan = orbit->RAAN();
    inclination = orbit->Inclination();

    // Gap is needed to eliminate different start times
    gap = orbit->TPlusEpoch(currentJulian);

    updateTime(targetTime);

    // Set name from TLE file for icon name
    line3 = orbit->SatName(false);
}

void NoradTLE::updateTime(const simtime_t& targetTime)
{
    orbit->getPosition((gap + targetTime.dbl()) / 60, &eci);
    geoCoord = eci.toGeo();
}

double NoradTLE::getRaan()
{
    return raan;
}

double NoradTLE::getInclination()
{
    return inclination;
}

void NoradTLE::handleMessage(cMessage* msg)
{
    error("Error in Norad::handleMessage(): This module is not able to handle messages.");
}
