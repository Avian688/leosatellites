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
#include<fstream>
#include <sstream>
#include<iostream>
#include <filesystem>
#include "LeoIpv4NetworkConfigurator.h"

namespace inet {
Define_Module(LeoIpv4NetworkConfigurator);

static void silent_warning_handler(const char *reason, const char *file, int line) {
    // Silence all warnings as the console will be filled with warnings if a ground station cannot connect to a satellite
}

LeoIpv4NetworkConfigurator::~LeoIpv4NetworkConfigurator()
{
    nodeModules.clear();
    igraph_vector_int_destroy(&islVec);
}

void LeoIpv4NetworkConfigurator::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        // Disable igraph warnings
        igraph_set_warning_handler(&silent_warning_handler);

        // Get reference to the parent module
        cModule *parent = getParentModule();

        // Read module and configuration parameters
        networkName = parent->getName();
        loadFiles = par("loadFiles");
        minLinkWeight = par("minLinkWeight");
        configureIsolatedNetworksSeparatly = par("configureIsolatedNetworksSeparatly").boolValue();

        // Read satellite network parameters from parent
        numOfSats = parent->par("numOfSats");
        numOfGS = parent->par("numOfGS");
        satPerPlane = parent->par("satsPerPlane");
        unsigned int planes = parent->par("numOfPlanes");

        // Calculate total number of planes (ensures all satellites are distributed)
        numOfPlanes = (int)std::ceil(((double)numOfSats / ((double)planes * (double)satPerPlane)) * (double)planes);

        // Set number of inter-satellite links (2 per satellite in a grid-like pattern)
        numOfISLs = numOfSats * 2;

        // Assign unique IDs to modules (custom method)
        assignIDtoModules();

        // Path calculation parameters
        numOfKPaths = par("numOfKPaths");
        currentInterval = 0;

        // Read orbit characteristics
        double altitude = parent->par("alt");
        double inclination = parent->par("incl");

        // Convert altitude and inclination to strings
        std::string altitudeStr = std::to_string(altitude);
        std::string inclinationStr = std::to_string(inclination);

        // Trim trailing zeros and a possible trailing dot from altitude
        altitudeStr.erase(altitudeStr.find_last_not_of('0') + 1);
        if (!altitudeStr.empty() && altitudeStr.back() == '.') {
            altitudeStr.pop_back();
        }

        // Trim trailing zeros and a possible trailing dot from inclination
        inclinationStr.erase(inclinationStr.find_last_not_of('0') + 1);
        if (!inclinationStr.empty() && inclinationStr.back() == '.') {
            inclinationStr.pop_back();
        }

        // Determine the topology tag based on ISL setting
        std::string topologyTag = parent->par("enableInterSatelliteLinks").boolValue() ? "_ISL" : "_BP";

        // Compose the file prefix using underscores as separators
        filePrefix = std::to_string(numOfSats) + "_" + altitudeStr + "_" + std::to_string(planes) + "_" + std::to_string(satPerPlane) + "_" + inclinationStr + "_" + std::to_string(numOfGS) + topologyTag;

        if(!loadFiles){
            try {
                if (std::filesystem::exists(filePrefix) && std::filesystem::is_directory(filePrefix)) {
                    std::filesystem::remove_all(filePrefix); // This deletes the folder and all its contents
                    std::cout << "Deleted folder: " << filePrefix << std::endl;
                } else {
                    std::cout << "Folder does not exist: " << filePrefix << std::endl;
                }
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "Filesystem error: " << e.what() << std::endl;
            }
        }

        writeModuleIDMappingsToFile(filePrefix + "/idMap.txt");

        verifyModuleIDMappingsFromFile(filePrefix + "/idMap.txt");

        igraph_vector_int_init(&islVec, 0);
    }
}

bool LeoIpv4NetworkConfigurator::loadConfiguration(simtime_t currentInterval)
{
    std::string fName = filePrefix + "/" + currentInterval.str() + ".bin";

    if (!std::filesystem::is_regular_file(fName))
        return false;

    std::ifstream file(fName, std::ios::in | std::ios::binary);
    if (!file.is_open())
        return false;

    static std::unordered_map<int, LeoIpv4*> cachedIpv4;

    while (true) {
        int nodeId;
        int destAddr;
        int nextHopId;

        // Read a record: 3 integers
        file.read(reinterpret_cast<char*>(&nodeId), sizeof(int));
        file.read(reinterpret_cast<char*>(&destAddr), sizeof(int));
        file.read(reinterpret_cast<char*>(&nextHopId), sizeof(int));

        if (file.eof() || file.fail())
            break;

        // Check cache
        LeoIpv4* ipv4Mod = nullptr;
        auto cached = cachedIpv4.find(nodeId);
        if (cached != cachedIpv4.end()) {
            ipv4Mod = cached->second;
        }
        else {
            auto it = nodeModules.find(nodeId);
            if (it == nodeModules.end())
                continue;
            cModule* nodeMod = it->second;
            ipv4Mod = dynamic_cast<LeoIpv4*>(nodeMod->getModuleByPath(".ipv4.ip"));
            if (!ipv4Mod)
                continue;
            cachedIpv4[nodeId] = ipv4Mod;
        }

        ipv4Mod->addKNextHop(1, destAddr, nextHopId);
        ipv4Mod->addNextHop(destAddr, nextHopId);
    }
    file.close();
    return true;
}

void LeoIpv4NetworkConfigurator::assignIDtoModules()
{
    for(int nodeNum = 0; nodeNum < numOfSats+numOfGS; nodeNum++){
        nodeModules[nodeNum] = getNodeModule(nodeNum);
    }
}

void LeoIpv4NetworkConfigurator::updateForwardingStates(simtime_t currentInterval)
{
    if(loadFiles){
        bool completedLoad = loadConfiguration(currentInterval);
        if(!completedLoad){
            std::cerr << "WARNING: Failed to load routing files at simtime: " << simTime() << " Make sure you generate routing files before using loadFile = false in ini file." << endl;
        }
    }
    else{
        generateTopologyGraph(currentInterval);
    }

}

void LeoIpv4NetworkConfigurator::establishInitialISLs()
{
    unsigned int islVecIterator = 0;
    for(int planeNum = 0; planeNum < numOfPlanes; planeNum++){
        unsigned int numOfSatsInPlane =  planeNum*satPerPlane+satPerPlane;
        if(numOfSats < numOfSatsInPlane){
            numOfSatsInPlane = numOfSats;
        }
        for(unsigned int satNum = planeNum*satPerPlane; satNum < numOfSatsInPlane; satNum++){
            cModule *satMod = nodeModules.find(satNum)->second; // get source satellite module
            SatelliteMobility* sourceSatMobility = dynamic_cast<SatelliteMobility*>(satMod->getSubmodule("mobility"));
            int destSatNumA = (satNum+1)%(satPerPlane*(planeNum+1));
            if(destSatNumA == 0){
                destSatNumA = planeNum*satPerPlane; //If number is zero, must be start of orbital plane
            }
            if(destSatNumA < numOfSats){
                cModule *destModA = nodeModules.find(destSatNumA)->second;
                //VECTOR(islVec)[islVecIterator] = satNum; VECTOR(islVec)[islVecIterator+1] = destSatNumA;
                igraph_vector_int_push_back(&islVec, satNum);
                igraph_vector_int_push_back(&islVec, destSatNumA);

                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModA->getModuleByPath(".mobility"));
                satelliteISLMobilityModules[sourceSatMobility].push_back(destSatMobility);

                islVecIterator = islVecIterator + 2;
//                for(int i = 0; i < satMod->gateSize("pppg$o"); i++){  //check each possible pppg gate
//                    cGate* srcGate = satMod->gate("pppg$o", i);
//                    if(srcGate->isConnected()){
//                        cChannel *chan = srcGate->getChannel();
//                        cModule* destModule = srcGate->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
//                        //std::cout << "\n" << destModA->getFullName() << " - " << destModule->getFullName() << endl;
//                        if(destModA == destModule){
//                            std::string mobilityName = destModA->getModuleByPath(".mobility")->getNedTypeName();
//                            double distance = 0;
//                            if(mobilityName == "leosatellites.mobility.SatelliteMobility"){
//                                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModA->getModuleByPath(".mobility"));
//                                satelliteISLMobilityModules[sourceSatMobility].push_back(destSatMobility);
//                            }
//                        }
//                    }
//                }
            }
            int destSatNumB = (satNum + satPerPlane);// % totalSats;
            if(destSatNumB < numOfSats){
                cModule *destModB = nodeModules.find(destSatNumB)->second;
                //VECTOR(islVec)[islVecIterator] = satNum; VECTOR(islVec)[islVecIterator+1] = destSatNumB;
                igraph_vector_int_push_back(&islVec, satNum);
                igraph_vector_int_push_back(&islVec, destSatNumB);

                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModB->getModuleByPath(".mobility"));
                satelliteISLMobilityModules[sourceSatMobility].push_back(destSatMobility);

                //islVecIterator = islVecIterator + 2;
//                for(int i = 0; i < satMod->gateSize("pppg$o"); i++){  //check each possible pppg gate
//                    cGate* srcGate = satMod->gate("pppg$o", i);
//                    if(srcGate->isConnected()){
//                        cChannel *chan = srcGate->getChannel();
//                        cModule* destModule = srcGate->getPathEndGate()->getOwnerModule()->getParentModule()->getParentModule();
//                        if(destModB == destModule){
//                            std::string mobilityName = destModB->getModuleByPath(".mobility")->getNedTypeName();
//                            double distance = 0;
//                            if(mobilityName == "leosatellites.mobility.SatelliteMobility"){
//                                SatelliteMobility* destSatMobility = dynamic_cast<SatelliteMobility*>(destModB->getModuleByPath(".mobility"));
//                                satelliteISLMobilityModules[sourceSatMobility].push_back(destSatMobility);
//                            }
//                        }
//                    }
//                }
            }
        }
    }
}

void LeoIpv4NetworkConfigurator::generateTopologyGraph(simtime_t currentInterval)
{
    if (!std::filesystem::is_directory(filePrefix) || !std::filesystem::exists(filePrefix)) {
        std::filesystem::create_directory(filePrefix);
    }
    std::ofstream fout;
    std::string fName = filePrefix + "/" + currentInterval.str() + ".bin";
    fout.open(fName, std::ios::binary | std::ios::app);

    for (int nodeNum = 0; nodeNum < numOfSats+numOfGS; nodeNum++) {
        cModule* mod = nodeModules.find(nodeNum)->second;
        dynamic_cast<LeoIpv4 *>(mod->getModuleByPath(".ipv4.ip"))->clearNextHops();
    }
    igraph_t constellationTopology;
    igraph_vector_int_t shortestPathVertexVec, shortestPathEdgesVec;
    igraph_vector_int_t islVecCopy;
    igraph_vector_int_init_copy(&islVecCopy, &islVec);
    //std::cout << "\nOG ISL VEC SIZE: " << igraph_vector_int_size(&islVec) << endl;
    igraph_vector_t weightsVec;
    igraph_vector_init(&weightsVec, 0);
    unsigned int islVecIterator = 0;
    unsigned int weightsVecIterator = 0;
    for (auto iter = satelliteISLMobilityModules.begin(); iter != satelliteISLMobilityModules.end(); ++iter) {
        std::vector<SatelliteMobility*> mobVec = iter->second;
        for (SatelliteMobility* iter2 : mobVec) {
            //if(iter->first != iter2){
                //std::cout << "\nSOURCE SAT: " << iter->first->getParentModule()->getFullName() << "\nDEST SAT: " << iter2->getParentModule()->getFullName();
                double distance = iter->first->getDistance(iter2->getLatitude(), iter2->getLongitude(), iter2->getAltitude())*1000;
                double weight = (distance/299792458)*1000;
                //VECTOR(weightsVec)[weightsVecIterator] = weight;
                igraph_vector_push_back(&weightsVec, weight);
                weightsVecIterator++;
                islVecIterator = islVecIterator + 2;
            //}
        }
    }
    int numberOfGSLinks = groundStationLinks.size();
    for(int i = 0; i < numberOfGSLinks; i++){ //TODO USE ATTRIBUTE NAMES TO
        std::tuple<int, int, double> gsTup = groundStationLinks.front();
        groundStationLinks.pop();
        igraph_vector_int_push_back(&islVecCopy, std::get<0>(gsTup)); // @suppress("Function cannot be instantiated")
        igraph_vector_int_push_back(&islVecCopy, std::get<1>(gsTup));
        //VECTOR(weightsVec)[weightsVecIterator] = std::get<2>(gsTup);
        igraph_vector_push_back(&weightsVec, std::get<2>(gsTup));
        islVecIterator = islVecIterator + 2;
        weightsVecIterator++;
    }

    igraph_empty(&constellationTopology, numOfSats+numOfGS, IGRAPH_UNDIRECTED);

    igraph_add_edges(&constellationTopology, &islVecCopy, 0);

    igraph_vector_int_init(&shortestPathVertexVec, 1);
    igraph_vector_int_init(&shortestPathEdgesVec, 1);

    igraph_vector_int_list_t vertexPaths;
    igraph_vector_int_list_t edgePaths;

    igraph_vector_int_list_init(&vertexPaths, 1);
    igraph_vector_int_list_init(&edgePaths, 1);

    if(numOfKPaths <= 1){
        int fmDuration = 0;
        int rDuration = 0;
        for(int sourceNodeNum = 0; sourceNodeNum < numOfSats+numOfGS; sourceNodeNum++){
            igraph_get_all_shortest_paths_dijkstra(&constellationTopology, &vertexPaths, &edgePaths, /*nrgeo=*/ &shortestPathVertexVec, /*from=*/ sourceNodeNum, /*to=*/ igraph_vss_all(), /*weights=*/ &weightsVec, /*mode=*/ IGRAPH_ALL);
            cModule *sourceMod = nodeModules.find(sourceNodeNum)->second;
            LeoIpv4* srcIpv4Mod = dynamic_cast<LeoIpv4 *>(sourceMod->getModuleByPath(".ipv4.ip"));
            igraph_vector_int_t *path;
            for (int i = 0; i < igraph_vector_int_list_size(&vertexPaths); i++) {
                path = igraph_vector_int_list_get_ptr(&vertexPaths, i);
                int sourceNodeNum = igraph_vector_int_get(path, 0);
                int nextHopNodeNum = igraph_vector_int_get(path, 1);
                int destinationNodeNum = igraph_vector_int_get(path, igraph_vector_int_size(path)-1);
                cModule *nextHopMod = nodeModules.find(nextHopNodeNum)->second;
                cModule *destMod = nodeModules.find(destinationNodeNum)->second;
                if(sourceNodeNum != destinationNodeNum){
                    int nextHopID = nextHopInterfaceMap.find(sourceMod)->second.find(nextHopMod)->second;
//                    IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(destMod->getSubmodule("interfaceTable"));
//                    int totalInterfaces = 0;
//                    for (size_t j = 0; j < destIft->getNumInterfaces(); j++) {
//                        NetworkInterface *destinationIE = destIft->getInterface(j);
//                        if (!destinationIE->isLoopback()){
////                            srcIpv4Mod->addNextHop(destinationIE->getIpv4Address().getInt(),nextHopID);
////                            std::string str1 = destMod->getFullName();
////                            std::string str2 = nextHopMod->getFullName() + std::to_string(nextHopID);
////                            //str2 = str2 + std::string(nextHopID);
////                            srcIpv4Mod->addNextHopStr(str1, str2);
////                            //srcIpv4Mod->addKNextHop(1, destinationIE->getIpv4Address().getInt(), nextHopID);
////                            int32_t src = sourceNodeNum;
////                            int32_t dst = destinationIE->getIpv4Address().getInt();
////                            int32_t nhop = nextHopID;
//                        }
//                        else{
//                            totalInterfaces = totalInterfaces + 1;
//                        }
//                    }
                    srcIpv4Mod->addKNextHop(1, destinationNodeNum, nextHopID);
                    fout.write(reinterpret_cast<const char*>(&sourceNodeNum), sizeof(int32_t));
                    fout.write(reinterpret_cast<const char*>(&destinationNodeNum), sizeof(int32_t));
                    fout.write(reinterpret_cast<const char*>(&nextHopID), sizeof(int32_t));
                }
            }
            igraph_vector_int_destroy(path);
            igraph_vector_int_list_clear(&vertexPaths);
            igraph_vector_int_list_clear(&edgePaths);
        }
    }
    else{
        for(int sourceNodeNum = 0; sourceNodeNum < numOfSats+numOfGS; sourceNodeNum++){
            cModule *sourceMod = nodeModules.find(sourceNodeNum)->second;
            LeoIpv4* srcIpv4Mod = dynamic_cast<LeoIpv4 *>(sourceMod->getModuleByPath(".ipv4.ip"));
            for(int destNodeNum = 0; destNodeNum <  numOfSats+numOfGS; destNodeNum++){
                if(sourceNodeNum != destNodeNum){
                    igraph_get_k_shortest_paths(&constellationTopology, &weightsVec, &vertexPaths, &edgePaths, numOfKPaths, sourceNodeNum, destNodeNum, IGRAPH_ALL);
                    igraph_vector_int_t *path;
                    for (int i = 0; i < igraph_vector_int_list_size(&vertexPaths); i++) {
                        path = igraph_vector_int_list_get_ptr(&vertexPaths, i);
                        int sourceNodeNum = igraph_vector_int_get(path, 0);
                        int nextHopNodeNum = igraph_vector_int_get(path, 1);
                        int destinationNodeNum = igraph_vector_int_get(path, igraph_vector_int_size(path)-1);
                        cModule *nextHopMod = nodeModules.find(nextHopNodeNum)->second;
                        cModule *destMod = nodeModules.find(destinationNodeNum)->second;
                        if(sourceNodeNum != destinationNodeNum){
                            int nextHopID = nextHopInterfaceMap.find(sourceMod)->second.find(nextHopMod)->second;
//                            IInterfaceTable* destIft = dynamic_cast<IInterfaceTable*>(destMod->getSubmodule("interfaceTable"));
//                            for (size_t j = 0; j < destIft->getNumInterfaces(); j++) {
//                                NetworkInterface *destinationIE = destIft->getInterface(j);
//                                if (!destinationIE->isLoopback()){
//                                    srcIpv4Mod->addNextHop(destinationIE->getIpv4Address().getInt(),nextHopID);
//                                    srcIpv4Mod->addKNextHop(i+1, destinationIE->getIpv4Address().getInt(), nextHopID);
//
//                                    int32_t src = sourceNodeNum;
//                                    int32_t dst = destinationIE->getIpv4Address().getInt();
//                                    int32_t nhop = nextHopID;
//                                    fout.write(reinterpret_cast<const char*>(&src), sizeof(int32_t));
//                                    fout.write(reinterpret_cast<const char*>(&dst), sizeof(int32_t));
//                                    fout.write(reinterpret_cast<const char*>(&nhop), sizeof(int32_t));
//                                }
//                            }
                            srcIpv4Mod->addKNextHop(i+1, destinationNodeNum, nextHopID);
                            fout.write(reinterpret_cast<const char*>(&sourceNodeNum), sizeof(int32_t));
                            fout.write(reinterpret_cast<const char*>(&destinationNodeNum), sizeof(int32_t));
                            fout.write(reinterpret_cast<const char*>(&nextHopID), sizeof(int32_t));
                        }
                    }
                    igraph_vector_int_destroy(path);
                    igraph_vector_int_list_clear(&vertexPaths);
                    igraph_vector_int_list_clear(&edgePaths);
                }
            }
        }
    }
    igraph_vector_int_list_destroy(&vertexPaths);
    igraph_vector_int_list_destroy(&edgePaths);

    while(!groundStationLinks.empty()) groundStationLinks.pop();
    igraph_destroy(&constellationTopology);
    igraph_vector_int_destroy(&islVecCopy);
    igraph_vector_destroy(&weightsVec);
    igraph_vector_int_destroy(&shortestPathVertexVec);
    igraph_vector_int_destroy(&shortestPathEdgesVec);
    fout.close();
}

void LeoIpv4NetworkConfigurator::addNextHopInterface(cModule* source, cModule* destination, int interfaceID)
{
    nextHopInterfaceMap[source][destination] = interfaceID;
}

void LeoIpv4NetworkConfigurator::removeNextHopInterface(cModule* source, cModule* destination)
{
    nextHopInterfaceMap[source].erase(destination);
}

void LeoIpv4NetworkConfigurator::addGSLinktoTopologyGraph(int sourceNum, int destNum, double weight)
{
    groundStationLinks.push(std::tuple<int, int, double>(sourceNum, destNum, weight));
}

cModule* LeoIpv4NetworkConfigurator::getNodeModule(int nodeNumber)
{
    cModule* mod;
    if(nodeNumber < numOfSats){
        std::string nodeName = std::string(networkName + ".satellite[" + std::to_string(nodeNumber) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    else{
        std::string nodeName = std::string(networkName + ".groundStation[" + std::to_string(nodeNumber-numOfSats) + "]");
        mod = getModuleByPath(nodeName.c_str());
    }
    return mod;
}

#include <fstream>
#include <filesystem>
#include <map>

void LeoIpv4NetworkConfigurator::writeModuleIDMappingsToFile(const std::string& filePath)
{
    namespace fs = std::filesystem;

    // Declare outFile here to be in scope
    std::ofstream outFile;

    if (!loadFiles) {
        // Create folder if necessary
        fs::path pathObj(filePath);
        fs::path dir = pathObj.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            if (!fs::create_directories(dir)) {
                std::cout << "Error: Could not create directory " << dir << "\n";
                return;
            }
        }

        outFile.open(filePath);
        if (!outFile.is_open()) {
            EV << "Error: Could not open file " << filePath << " for writing.\n";
            return;
        }
    }

    // Write module mappings and populate the dictionary
    for (int nodeNum = 0; nodeNum < numOfSats + numOfGS; ++nodeNum) {
        std::string nodeName;
        if (nodeNum < numOfSats) {
            nodeName = "satellite[" + std::to_string(nodeNum) + "]";
        } else {
            nodeName = "groundStation[" + std::to_string(nodeNum - numOfSats) + "]";
        }

        int moduleId = nodeModules[nodeNum] ? nodeModules[nodeNum]->getId() : -1;

        // Add to dictionary
        moduleIDMap[nodeName] = moduleId;
        moduleGraphIDMap[nodeName] = nodeNum;

        // Write to file if loadFiles is false
        if (!loadFiles) {
            outFile << nodeName << " = ";
            if (moduleId != -1) {
                outFile << moduleId;
            } else {
                outFile << "NULL";
            }
            outFile << "\n";
        }
    }

    // Close file if loadFiles is false
    if (!loadFiles) {
        outFile.close();
        EV << "Module ID mappings written to " << filePath << "\n";
    }

}

void LeoIpv4NetworkConfigurator::verifyModuleIDMappingsFromFile(const std::string& filePath)
{
    if(!loadFiles){
       return;
    }

    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << " for reading.\n";
        return;
    }

    std::string line;
    bool allMatch = true;

    while (std::getline(inFile, line)) {
        std::istringstream iss(line);
        std::string nodeName;
        std::string equalsSign;
        std::string idStr;

        if (!(iss >> nodeName >> equalsSign >> idStr)) {
            std::cout << "Warning: Malformed line: " << line << "\n";
            continue;
        }

        // Remove potential trailing whitespace or newline characters
        nodeName.erase(nodeName.find_last_not_of(" \n\r\t") + 1);
        idStr.erase(idStr.find_last_not_of(" \n\r\t") + 1);

        int fileId = (idStr == "NULL") ? -1 : std::stoi(idStr);

        // Find in private map
        auto it = moduleIDMap.find(nodeName);
        if (it == moduleIDMap.end()) {
            std::cout << "Mismatch: " << nodeName << " not found in internal map.\n";
            allMatch = false;
        } else if (it->second != fileId) {
            std::cout << "Mismatch: " << nodeName << " = " << it->second << " (in map), "
               << fileId << " (in file)\n";
            allMatch = false;
        }
    }

    inFile.close();

    if (allMatch) {
        std::cout << "Module ID mappings in file match internal map.\n";
    }
}

int LeoIpv4NetworkConfigurator::getNextHopInterfaceID(cModule* sourceSatellite, cModule* nextHopSatellite)
{
    int interfaceID = -1;
    if(sourceSatellite == nextHopSatellite){
        return interfaceID;
    }

    IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(sourceSatellite->getSubmodule("interfaceTable"));
    IInterfaceTable* nextHopIft = dynamic_cast<IInterfaceTable*>(nextHopSatellite->getSubmodule("interfaceTable"));

    for (int i = 0; i < sourceIft->getNumInterfaces(); i++) {
        NetworkInterface *srcIE = sourceIft->getInterface(i);
        if (!(srcIE->isPointToPoint()))
            continue;
        cGate* srcGateMod = sourceSatellite->gate(srcIE->getNodeOutputGateId());
        for (int j = 0; j < nextHopIft->getNumInterfaces(); j++) {
            NetworkInterface *nextHopIE = nextHopIft->getInterface(j);
            if (!(nextHopIE->isPointToPoint()))
                continue;
            cGate* nextHopGateMod = nextHopSatellite->gate(nextHopIE->getNodeInputGateId());
            if(srcGateMod->getPathEndGate() == nextHopGateMod->getPathEndGate()){
                return srcIE->getInterfaceId();
            }
        }
    }
    return interfaceID;
}

void LeoIpv4NetworkConfigurator::fillNextHopInterfaceMap()
{
    for(int nodeNum = 0; nodeNum < nodeModules.size(); nodeNum++){
        cModule *mod = nodeModules.find(nodeNum)->second;
        for(int nextHopNodeNum = 0; nextHopNodeNum < nodeModules.size(); nextHopNodeNum++){
            if(nodeNum != nextHopNodeNum){
                cModule *nextHopMod = nodeModules.find(nextHopNodeNum)->second;
                IInterfaceTable* sourceIft = dynamic_cast<IInterfaceTable*>(mod->getSubmodule("interfaceTable"));
                IInterfaceTable* nextHopIft = dynamic_cast<IInterfaceTable*>(nextHopMod->getSubmodule("interfaceTable"));

                for (int i = 0; i < sourceIft->getNumInterfaces(); i++) {
                    NetworkInterface *srcIE = sourceIft->getInterface(i);
                    if (!(srcIE->isPointToPoint())){
                        addIpAddressMap(srcIE->getIpv4Address().getInt(), mod->getFullName());
                        continue;
                    }
                    cGate* srcGateMod = mod->gate(srcIE->getNodeOutputGateId());
                    for (int j = 0; j < nextHopIft->getNumInterfaces(); j++) {
                        NetworkInterface *nextHopIE = nextHopIft->getInterface(j);
                        if (!(nextHopIE->isPointToPoint())){
                            continue;
                        }
                        cGate* nextHopGateMod = nextHopMod->gate(nextHopIE->getNodeInputGateId());
                        if(srcGateMod->getPathEndGate() == nextHopGateMod->getPathEndGate()){
                            nextHopInterfaceMap[mod][nextHopMod] = srcIE->getInterfaceId();
                            addIpAddressMap(srcIE->getIpv4Address().getInt(), mod->getFullName());

                            if(srcIE->getIpv4Address().getInt() == 167932916){
                                std::cout << "\n FOUND HIM (map thinks this is groundStation[5]" << endl;
                                std::cout << "\n ACTUAL MOD: " << mod->getFullName() << endl;
                            }
                        }
                    }
                }
            }
        }
    }
}

double LeoIpv4NetworkConfigurator::computeLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
        return computeWiredLinkWeight(link, metric, nullptr);
}

double LeoIpv4NetworkConfigurator::computeWiredLinkWeight(Link *link, const char *metric, cXMLElement *parameters)
{
    //std::cout << "\n Metric: " << metric << endl;
    Topology::Link *linkOut = static_cast<Topology::Link *>(static_cast<Topology::Link *>(link));
    if (!strcmp(metric, "hopCount"))
        return 1;
    else if (!strcmp(metric, "delay")) {
        cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLinkOutLocalGate()->findTransmissionChannel());
        if (transmissionChannel != nullptr){
            return transmissionChannel->getDelay().dbl();
        }
        else
            return minLinkWeight;
    }
    else if (!strcmp(metric, "dataRate")) {
        cChannel *transmissionChannel = linkOut->getLinkOutLocalGate()->findTransmissionChannel();
        if (transmissionChannel != nullptr) {
            double dataRate = transmissionChannel->getNominalDatarate();
            return dataRate != 0 ? 1 / dataRate : minLinkWeight;
        }
        else
            return minLinkWeight;
    }
    else if (!strcmp(metric, "errorRate")) {
        cDatarateChannel *transmissionChannel = dynamic_cast<cDatarateChannel *>(linkOut->getLinkOutLocalGate()->findTransmissionChannel());
        if (transmissionChannel != nullptr) {
            InterfaceInfo *sourceInterfaceInfo = link->sourceInterfaceInfo;
            double bitErrorRate = transmissionChannel->getBitErrorRate();
            double packetErrorRate = 1.0 - pow(1.0 - bitErrorRate, sourceInterfaceInfo->networkInterface->getMtu());
            return minLinkWeight - log(1 - packetErrorRate);
        }
        else
            return minLinkWeight;
    }
    else
        throw cRuntimeError("Unknown metric");
}

int LeoIpv4NetworkConfigurator::getModuleIdFromIpAddress(int address)
{
    return routerIdMap[address];
}

void LeoIpv4NetworkConfigurator::addIpAddressMap(int address, std::string modName)
{
    routerIdMap[address] = moduleGraphIDMap[modName];
}

void LeoIpv4NetworkConfigurator::eraseIpAddressMap(int address)
{
    routerIdMap.erase(address);
}

int LeoIpv4NetworkConfigurator::getNodeModuleGraphId(std::string nodeStr)
{
    return moduleGraphIDMap[nodeStr];
}

// TODO If we set IPAddress in LeoChannelConstructor, do we need this? doubt it
//void LeoIpv4NetworkConfigurator::configureInterface(InterfaceInfo *interfaceInfo)
//{
//    EV_DETAIL << "Configuring network interface " << interfaceInfo->getFullPath() << ".\n";
//    InterfaceEntry *interfaceEntry = interfaceInfo->interfaceEntry;
//    Ipv4InterfaceData *interfaceData = interfaceEntry->getProtocolData<Ipv4InterfaceData>();
//    if (interfaceInfo->mtu != -1)
//        interfaceEntry->setMtu(interfaceInfo->mtu);
//    if (interfaceInfo->metric != -1)
//        interfaceData->setMetric(interfaceInfo->metric);
//    if (assignAddressesParameter) {
//        interfaceData->setIPAddress(Ipv4Address(interfaceInfo->address));
//        interfaceData->setNetmask(Ipv4Address(interfaceInfo->netmask));
//    }
//    // TODO: should we leave joined multicast groups first?
//    for (auto & multicastGroup : interfaceInfo->multicastGroups)
//        interfaceData->joinMulticastGroup(multicastGroup);
//}
}

