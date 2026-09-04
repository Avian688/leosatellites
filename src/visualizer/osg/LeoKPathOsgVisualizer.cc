#include "LeoKPathOsgVisualizer.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef WITH_OSG
#include <osg/LineWidth>
#include <osg/Matrix>
#include <osg/Point>
#include <osg/PrimitiveSet>
#include <osg/StateSet>
#include <osg/Texture2D>
#include <osgDB/ReadFile>
#endif

namespace inet {

Define_Module(LeoKPathOsgVisualizer);

LeoKPathOsgVisualizer::~LeoKPathOsgVisualizer()
{
    if (updateTimer != nullptr)
        cancelAndDelete(updateTimer);
}

void LeoKPathOsgVisualizer::initialize(int stage)
{
#ifndef WITH_OSG
    if (stage == 0)
        throw omnetpp::cRuntimeError(
            "LeoKPathOsgVisualizer requires an OMNeT++ build with WITH_OSG=yes");
#else
    if (stage == 0) {
        numSatellites = par("numSatellites");
        numGroundStations = par("numGroundStations");
        numClients = par("numClients");
        numUserTerminals = par("numUserTerminals");
        satellitesPerPlane = par("satellitesPerPlane");
        pathCount = par("pathCount");
        pairIndex = par("pairIndex");
        maxSharedLinks = par("maxSharedLinks");
        edgeDisjoint = par("edgeDisjoint");
        showInterSatelliteLinks = par("showInterSatelliteLinks");
        updateInterval = par("updateInterval");
        policyLabel = par("policyLabel").stdstringValue();

        if (pathCount < 1 || pathCount > 10)
            throw omnetpp::cRuntimeError("pathCount must be between 1 and 10");
        if (showInterSatelliteLinks && satellitesPerPlane < 1)
            throw omnetpp::cRuntimeError("satellitesPerPlane must be positive when ISLs are shown");
        if (updateInterval <= omnetpp::SimTime::ZERO)
            throw omnetpp::cRuntimeError("updateInterval must be positive");
        if (edgeDisjoint && maxSharedLinks != -1)
            throw omnetpp::cRuntimeError("maxSharedLinks must be -1 for the edge-disjoint policy");

        parseEndpointPairs();
        if (pairIndex < 0 || pairIndex >= static_cast<int>(terminalPairs.size()))
            throw omnetpp::cRuntimeError("pairIndex %d is outside the configured endpoint-pair range 0..%d",
                                        pairIndex, static_cast<int>(terminalPairs.size()) - 1);

        initializeScene();
        discoverSnapshots();
    }
    else if (stage == 1) {
        initializeConstellation();
        initializeInterSatelliteLinks();
        initializeStaticNodes();
        initializeLegend();
        updateVisualization();

        updateTimer = new omnetpp::cMessage("update K-path visualization");
        scheduleAt(simTime() + updateInterval, updateTimer);
    }
#endif
}

void LeoKPathOsgVisualizer::handleMessage(omnetpp::cMessage *message)
{
#ifdef WITH_OSG
    if (message != updateTimer)
        throw omnetpp::cRuntimeError("LeoKPathOsgVisualizer only accepts its update timer");

    updateVisualization();
    scheduleAt(simTime() + updateInterval, updateTimer);
#else
    throw omnetpp::cRuntimeError("LeoKPathOsgVisualizer is unavailable without OpenSceneGraph");
#endif
}

#ifdef WITH_OSG

void LeoKPathOsgVisualizer::initializeScene()
{
    osg::ref_ptr<osg::Image> earthImage = osgDB::readImageFile(par("earthTexture").stringValue());
    if (!earthImage)
        throw omnetpp::cRuntimeError("Could not load Earth texture '%s'",
                                    par("earthTexture").stringValue());

    scene = new osg::Group();
    osg::ref_ptr<osg::Geode> earth = new osg::Geode();
    earth->addDrawable(createEarthGeometry());
    osg::ref_ptr<osg::Texture2D> texture = new osg::Texture2D();
    texture->setImage(earthImage);
    texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR_MIPMAP_LINEAR);
    texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
    texture->setWrap(osg::Texture::WRAP_S, osg::Texture::REPEAT);
    earth->getOrCreateStateSet()->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);
    earth->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    scene->addChild(earth);

    osg::StateSet *stateSet = scene->getOrCreateStateSet();
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setMode(GL_CULL_FACE, osg::StateAttribute::ON);

    omnetpp::cOsgCanvas *canvas = getParentModule()->getOsgCanvas();
    canvas->setCameraManipulatorType(omnetpp::cOsgCanvas::CAM_TRACKBALL);
    canvas->setViewerStyle(omnetpp::cOsgCanvas::STYLE_GENERIC);
    canvas->setClearColor(omnetpp::cOsgCanvas::Color("black"));
    canvas->setZNear(1000);
    canvas->setZFar(1000000000);
    const double distance = par("cameraDistance").doubleValueInUnit("m");
    osg::Vec3d eyeDirection;
    if (par("autoCenterSelectedPair")) {
        const auto [sourceIndex, destinationIndex] = terminalPairs[pairIndex];
        omnetpp::cModule *source = getParentModule()->getSubmodule("userTerminal", sourceIndex);
        omnetpp::cModule *destination = getParentModule()->getSubmodule("userTerminal", destinationIndex);
        eyeDirection = geodeticToWorld(source->par("longitude"), source->par("latitude"), 0);
        osg::Vec3d destinationDirection =
            geodeticToWorld(destination->par("longitude"), destination->par("latitude"), 0);
        eyeDirection.normalize();
        destinationDirection.normalize();
        eyeDirection += destinationDirection;
        if (eyeDirection.length2() < 1e-12)
            eyeDirection = geodeticToWorld(source->par("longitude"), source->par("latitude"), 0);
        eyeDirection.normalize();
    }
    else {
        constexpr double PI_VALUE = 3.14159265358979323846;
        const double longitude = par("cameraLongitude").doubleValue() * PI_VALUE / 180.0;
        const double latitude = par("cameraLatitude").doubleValue() * PI_VALUE / 180.0;
        eyeDirection.set(std::cos(latitude) * std::cos(longitude),
                         std::cos(latitude) * std::sin(longitude),
                         std::sin(latitude));
    }
    const omnetpp::cOsgCanvas::Vec3d eye(
        distance * eyeDirection.x(), distance * eyeDirection.y(), distance * eyeDirection.z());
    canvas->setGenericViewpoint(omnetpp::cOsgCanvas::Viewpoint(
        eye, omnetpp::cOsgCanvas::Vec3d(0, 0, 0), omnetpp::cOsgCanvas::Vec3d(0, 0, 1)));
    canvas->setScene(scene);

    getParentModule()->getCanvas()->setAnimationSpeed(par("animationSpeed").doubleValue(), this);
}

osg::ref_ptr<osg::Geometry> LeoKPathOsgVisualizer::createEarthGeometry() const
{
    constexpr int LATITUDE_SEGMENTS = 64;
    constexpr int LONGITUDE_SEGMENTS = 128;

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec2Array> textureCoordinates = new osg::Vec2Array();
    vertices->reserve((LATITUDE_SEGMENTS + 1) * (LONGITUDE_SEGMENTS + 1));
    normals->reserve(vertices->capacity());
    textureCoordinates->reserve(vertices->capacity());

    for (int latitudeIndex = 0; latitudeIndex <= LATITUDE_SEGMENTS; ++latitudeIndex) {
        const double latitude = -90.0 + 180.0 * latitudeIndex / LATITUDE_SEGMENTS;
        for (int longitudeIndex = 0; longitudeIndex <= LONGITUDE_SEGMENTS; ++longitudeIndex) {
            const double longitude = -180.0 + 360.0 * longitudeIndex / LONGITUDE_SEGMENTS;
            const osg::Vec3d position = geodeticToWorld(longitude, latitude, 0);
            vertices->push_back(osg::Vec3(
                static_cast<float>(position.x()),
                static_cast<float>(position.y()),
                static_cast<float>(position.z())));
            osg::Vec3d normal = position;
            normal.normalize();
            normals->push_back(osg::Vec3(normal.x(), normal.y(), normal.z()));
            textureCoordinates->push_back(osg::Vec2(
                static_cast<float>(longitudeIndex) / LONGITUDE_SEGMENTS,
                static_cast<float>(latitudeIndex) / LATITUDE_SEGMENTS));
        }
    }

    osg::ref_ptr<osg::DrawElementsUInt> indices = new osg::DrawElementsUInt(GL_TRIANGLES);
    indices->reserve(LATITUDE_SEGMENTS * LONGITUDE_SEGMENTS * 6);
    for (int latitudeIndex = 0; latitudeIndex < LATITUDE_SEGMENTS; ++latitudeIndex) {
        for (int longitudeIndex = 0; longitudeIndex < LONGITUDE_SEGMENTS; ++longitudeIndex) {
            const unsigned int lowerLeft = latitudeIndex * (LONGITUDE_SEGMENTS + 1) + longitudeIndex;
            const unsigned int lowerRight = lowerLeft + 1;
            const unsigned int upperLeft = lowerLeft + LONGITUDE_SEGMENTS + 1;
            const unsigned int upperRight = upperLeft + 1;
            indices->push_back(lowerLeft);
            indices->push_back(lowerRight);
            indices->push_back(upperRight);
            indices->push_back(lowerLeft);
            indices->push_back(upperRight);
            indices->push_back(upperLeft);
        }
    }

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setVertexArray(vertices);
    geometry->setNormalArray(normals, osg::Array::BIND_PER_VERTEX);
    geometry->setTexCoordArray(0, textureCoordinates);
    geometry->addPrimitiveSet(indices);
    return geometry;
}

void LeoKPathOsgVisualizer::parseEndpointPairs()
{
    const std::string specification = par("endpointPairs").stdstringValue();
    omnetpp::cStringTokenizer tokenizer(specification.c_str(), ",");
    while (tokenizer.hasMoreTokens()) {
        const std::string token = tokenizer.nextToken();
        const size_t separator = token.find('-');
        if (separator == std::string::npos)
            throw omnetpp::cRuntimeError("Invalid endpoint pair '%s'; expected terminalIndex-terminalIndex",
                                        token.c_str());

        const int source = std::stoi(token.substr(0, separator));
        const int destination = std::stoi(token.substr(separator + 1));
        if (source < 0 || source >= numUserTerminals || destination < 0 || destination >= numUserTerminals)
            throw omnetpp::cRuntimeError("Endpoint pair '%s' refers to a missing user terminal", token.c_str());

        terminalPairs.emplace_back(source, destination);
    }
    if (terminalPairs.empty())
        throw omnetpp::cRuntimeError("endpointPairs must contain at least one pair");
}

void LeoKPathOsgVisualizer::discoverSnapshots()
{
    const int32_t endpointStart = numSatellites + numGroundStations + 2 * numClients;
    std::vector<std::pair<int32_t, int32_t>> graphPairs;
    graphPairs.reserve(terminalPairs.size());
    for (const auto& [source, destination] : terminalPairs)
        graphPairs.emplace_back(endpointStart + source, endpointStart + destination);

    const leoRouting::KPathAlgorithm algorithm =
        leoRouting::kPathAlgorithmForPolicy(edgeDisjoint, maxSharedLinks);
    const std::string profile = leoRouting::makeKPathSnapshotProfileName(
        algorithm, pathCount, par("maxRttSpread").doubleValueInUnit("ms"), maxSharedLinks, graphPairs);
    const std::filesystem::path directory =
        std::filesystem::path(par("routingDirectory").stringValue()) /
        "kpaths" / par("snapshotSet").stringValue() / profile;

    if (!std::filesystem::is_directory(directory))
        throw omnetpp::cRuntimeError(
            "K-path catalog directory '%s' does not exist; generate this policy before opening the viewer",
            directory.string().c_str());

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".bin")
            continue;
        try {
            const std::string stem = entry.path().stem().string();
            snapshotFiles.push_back({omnetpp::SimTime::parse(stem.c_str()), entry.path()});
        }
        catch (const std::exception&) {
            EV_WARN << "Ignoring K-path file with a non-time filename: " << entry.path() << std::endl;
        }
    }

    std::sort(snapshotFiles.begin(), snapshotFiles.end(), [](const SnapshotFile& left, const SnapshotFile& right) {
        return left.time < right.time;
    });
    if (snapshotFiles.empty())
        throw omnetpp::cRuntimeError("K-path catalog directory '%s' contains no readable snapshot filenames",
                                    directory.string().c_str());

    EV_INFO << "K-path 3D viewer loaded " << snapshotFiles.size()
            << " snapshot filenames from " << directory << std::endl;
}

void LeoKPathOsgVisualizer::initializeConstellation()
{
    satelliteOrbits.reserve(numSatellites);
    satellitePositions.resize(numSatellites);
    satelliteVertices = new osg::Vec3dArray();
    satelliteVertices->resize(numSatellites);

    std::time_t epoch = 1619119189;
    std::tm *epochUtc = std::gmtime(&epoch);
    if (epochUtc == nullptr)
        throw omnetpp::cRuntimeError("Could not initialize the constellation epoch");

    for (int index = 0; index < numSatellites; ++index) {
        omnetpp::cModule *satellite = getParentModule()->getSubmodule("satellite", index);
        if (satellite == nullptr)
            throw omnetpp::cRuntimeError("Missing satellite[%d]", index);
        auto *orbit = omnetpp::check_and_cast<::INorad *>(satellite->getSubmodule("NoradModule"));
        orbit->setJulian(epochUtc);
        orbit->initializeMobility(omnetpp::SimTime::ZERO);
        satelliteOrbits.push_back(orbit);
    }

    if (par("showSatellites")) {
        satelliteGeometry = createPointGeometry(
            satelliteVertices, osg::Vec4(0.72f, 0.78f, 0.86f, 0.8f),
            static_cast<float>(par("satellitePointSize").doubleValue()), true);
        osg::ref_ptr<osg::Geode> satelliteGeode = new osg::Geode();
        satelliteGeode->addDrawable(satelliteGeometry);
        scene->addChild(satelliteGeode);
    }
}

void LeoKPathOsgVisualizer::initializeInterSatelliteLinks()
{
    if (!showInterSatelliteLinks)
        return;

    interSatelliteLinks.reserve(2 * numSatellites);
    for (int satellite = 0; satellite < numSatellites; ++satellite) {
        const int plane = satellite / satellitesPerPlane;
        int nextInPlane = (satellite + 1) % (satellitesPerPlane * (plane + 1));
        if (nextInPlane == 0)
            nextInPlane = plane * satellitesPerPlane;
        if (nextInPlane < numSatellites)
            interSatelliteLinks.emplace_back(satellite, nextInPlane);

        const int nextPlane = satellite + satellitesPerPlane;
        if (nextPlane < numSatellites)
            interSatelliteLinks.emplace_back(satellite, nextPlane);
    }

    interSatelliteLinkVertices = new osg::Vec3Array();
    interSatelliteLinkVertices->resize(2 * interSatelliteLinks.size());
    interSatelliteLinkGeometry = new osg::Geometry();
    interSatelliteLinkGeometry->setVertexArray(interSatelliteLinkVertices);
    interSatelliteLinkGeometry->addPrimitiveSet(
        new osg::DrawArrays(GL_LINES, 0, interSatelliteLinkVertices->size()));

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(osg::Vec4(0.45f, 0.60f, 0.72f, 0.28f));
    interSatelliteLinkGeometry->setColorArray(colors, osg::Array::BIND_OVERALL);
    interSatelliteLinkGeometry->setDataVariance(osg::Object::DYNAMIC);
    interSatelliteLinkGeometry->setUseDisplayList(false);
    interSatelliteLinkGeometry->setUseVertexBufferObjects(true);

    osg::StateSet *stateSet = interSatelliteLinkGeometry->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::LineWidth(1.0f), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    stateSet->setRenderBinDetails(10, "RenderBin");

    osg::ref_ptr<osg::Geode> geode = new osg::Geode();
    geode->addDrawable(interSatelliteLinkGeometry);
    scene->addChild(geode);
}

void LeoKPathOsgVisualizer::initializeStaticNodes()
{
    const double groundAltitude = par("groundMarkerAltitude").doubleValueInUnit("m");
    const double terminalAltitude = par("terminalMarkerAltitude").doubleValueInUnit("m");

    groundStationPositions.reserve(numGroundStations);
    osg::ref_ptr<osg::Vec3dArray> groundVertices = new osg::Vec3dArray();
    for (int index = 0; index < numGroundStations; ++index) {
        omnetpp::cModule *node = getParentModule()->getSubmodule("groundStation", index);
        if (node == nullptr)
            throw omnetpp::cRuntimeError("Missing groundStation[%d]", index);
        const osg::Vec3d position = geodeticToWorld(
            node->par("longitude"), node->par("latitude"), groundAltitude);
        groundStationPositions.push_back(position);
        groundVertices->push_back(position);
    }

    userTerminalPositions.reserve(numUserTerminals);
    osg::ref_ptr<osg::Vec3dArray> terminalVertices = new osg::Vec3dArray();
    for (int index = 0; index < numUserTerminals; ++index) {
        omnetpp::cModule *node = getParentModule()->getSubmodule("userTerminal", index);
        if (node == nullptr)
            throw omnetpp::cRuntimeError("Missing userTerminal[%d]", index);
        const osg::Vec3d position = geodeticToWorld(
            node->par("longitude"), node->par("latitude"), terminalAltitude);
        userTerminalPositions.push_back(position);
        terminalVertices->push_back(position);
    }

    if (par("showGroundStations")) {
        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
        geode->addDrawable(createPointGeometry(
            groundVertices, osg::Vec4(0.1f, 0.85f, 1.0f, 0.95f),
            static_cast<float>(par("groundStationPointSize").doubleValue()), false));
        scene->addChild(geode);
    }

    if (par("showUserTerminals")) {
        osg::ref_ptr<osg::Geode> geode = new osg::Geode();
        geode->addDrawable(createPointGeometry(
            terminalVertices, osg::Vec4(1.0f, 0.72f, 0.1f, 1.0f),
            static_cast<float>(par("userTerminalPointSize").doubleValue()), false));
        scene->addChild(geode);
    }

    pathGeode = new osg::Geode();
    selectedEndpointGeode = new osg::Geode();
    scene->addChild(pathGeode);
    scene->addChild(selectedEndpointGeode);
}

osg::ref_ptr<osg::Geometry> LeoKPathOsgVisualizer::createPointGeometry(
    osg::Vec3dArray *vertices, const osg::Vec4& color, float pointSize, bool dynamic) const
{
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setVertexArray(vertices);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_POINTS, 0, vertices->size()));

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(color);
    geometry->setColorArray(colors, osg::Array::BIND_OVERALL);
    geometry->setDataVariance(dynamic ? osg::Object::DYNAMIC : osg::Object::STATIC);
    geometry->setUseDisplayList(!dynamic);
    geometry->setUseVertexBufferObjects(dynamic);

    osg::StateSet *stateSet = geometry->getOrCreateStateSet();
    stateSet->setAttributeAndModes(new osg::Point(pointSize), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    return geometry;
}

void LeoKPathOsgVisualizer::initializeLegend()
{
    osg::ref_ptr<osg::Camera> hud = new osg::Camera();
    hud->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    hud->setProjectionMatrix(osg::Matrix::ortho2D(0, 1920, 0, 1080));
    hud->setViewMatrix(osg::Matrix::identity());
    hud->setClearMask(0);
    hud->setRenderOrder(osg::Camera::POST_RENDER);
    hud->setAllowEventFocus(false);

    osg::StateSet *hudState = hud->getOrCreateStateSet();
    hudState->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    hudState->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    hudState->setMode(GL_BLEND, osg::StateAttribute::ON);

    osg::ref_ptr<osg::Geode> textGeode = new osg::Geode();
    headerText = new osgText::Text();
    headerText->setPosition(osg::Vec3(25, 1050, 0));
    headerText->setAlignment(osgText::Text::LEFT_TOP);
    headerText->setCharacterSize(25);
    headerText->setColor(osg::Vec4(1, 1, 1, 1));
    headerText->setBackdropType(osgText::Text::OUTLINE);
    headerText->setBackdropColor(osg::Vec4(0, 0, 0, 1));
    textGeode->addDrawable(headerText);

    for (int rank = 0; rank < pathCount; ++rank) {
        osg::ref_ptr<osgText::Text> text = new osgText::Text();
        text->setPosition(osg::Vec3(25, 970 - rank * 34, 0));
        text->setAlignment(osgText::Text::LEFT_TOP);
        text->setCharacterSize(21);
        text->setColor(pathColor(rank));
        text->setBackdropType(osgText::Text::OUTLINE);
        text->setBackdropColor(osg::Vec4(0, 0, 0, 1));
        rankTexts.push_back(text);
        textGeode->addDrawable(text);
    }

    hud->addChild(textGeode);
    scene->addChild(hud);
}

int LeoKPathOsgVisualizer::findSnapshotIndex(omnetpp::simtime_t time) const
{
    const auto iterator = std::upper_bound(
        snapshotFiles.begin(), snapshotFiles.end(), time,
        [](omnetpp::simtime_t value, const SnapshotFile& snapshot) { return value < snapshot.time; });
    if (iterator == snapshotFiles.begin())
        return 0;
    return static_cast<int>(std::distance(snapshotFiles.begin(), iterator) - 1);
}

void LeoKPathOsgVisualizer::updateVisualization()
{
    const int snapshotIndex = findSnapshotIndex(simTime());

    leoRouting::KPathSnapshot snapshot;
    try {
        snapshot = leoRouting::readKPathSnapshot(snapshotFiles[snapshotIndex].path);
    }
    catch (const std::exception& error) {
        throw omnetpp::cRuntimeError("Could not read K-path snapshot '%s': %s",
                                    snapshotFiles[snapshotIndex].path.string().c_str(), error.what());
    }

    const omnetpp::simtime_t snapshotTime(snapshot.header.timestampMicros, omnetpp::SIMTIME_US);
    for (int index = 0; index < numSatellites; ++index) {
        ::INorad *orbit = satelliteOrbits[index];
        orbit->updateTime(snapshotTime);
        satellitePositions[index] = geodeticToWorld(
            orbit->getLongitude(), orbit->getLatitude(), orbit->getAltitude() * 1000.0);
        (*satelliteVertices)[index] = satellitePositions[index];
    }
    satelliteVertices->dirty();
    if (satelliteGeometry)
        satelliteGeometry->dirtyBound();
    updateInterSatelliteLinks();

    const int32_t endpointStart = numSatellites + numGroundStations + 2 * numClients;
    const auto [sourceTerminal, destinationTerminal] = terminalPairs[pairIndex];
    const int32_t sourceNodeId = endpointStart + sourceTerminal;
    const int32_t destinationNodeId = endpointStart + destinationTerminal;
    const uint64_t selectedKey = leoRouting::kPathEndpointPairKey(sourceNodeId, destinationNodeId);

    auto group = std::find_if(snapshot.groups.begin(), snapshot.groups.end(), [&](const auto& candidate) {
        return leoRouting::kPathEndpointPairKey(candidate.sourceNodeId, candidate.destinationNodeId) == selectedKey;
    });
    if (group == snapshot.groups.end())
        throw omnetpp::cRuntimeError("Snapshot '%s' has no group for endpoint pair %d-%d",
                                    snapshotFiles[snapshotIndex].path.string().c_str(),
                                    sourceNodeId, destinationNodeId);

    pathGeode->removeDrawables(0, pathGeode->getNumDrawables());
    const int visiblePaths = std::min(pathCount, static_cast<int>(group->paths.size()));
    for (int rank = visiblePaths - 1; rank >= 0; --rank)
        pathGeode->addDrawable(createPathGeometry(group->paths[rank], rank));

    updateEndpointMarkers(sourceNodeId, destinationNodeId);
    updateLegend(*group, snapshotTime);

    EV_INFO << "Displaying " << policyLabel << ", pair " << pairIndex + 1
            << ", snapshot " << snapshotTime << ", " << visiblePaths << " paths" << std::endl;
}

void LeoKPathOsgVisualizer::updateInterSatelliteLinks()
{
    if (!interSatelliteLinkVertices)
        return;

    for (size_t index = 0; index < interSatelliteLinks.size(); ++index) {
        const auto [sourceIndex, destinationIndex] = interSatelliteLinks[index];
        const osg::Vec3d& source = satellitePositions[sourceIndex];
        const osg::Vec3d& destination = satellitePositions[destinationIndex];
        (*interSatelliteLinkVertices)[2 * index].set(
            static_cast<float>(source.x()),
            static_cast<float>(source.y()),
            static_cast<float>(source.z()));
        (*interSatelliteLinkVertices)[2 * index + 1].set(
            static_cast<float>(destination.x()),
            static_cast<float>(destination.y()),
            static_cast<float>(destination.z()));
    }
    interSatelliteLinkVertices->dirty();
    interSatelliteLinkGeometry->dirtyBound();
}

osg::ref_ptr<osg::Geometry> LeoKPathOsgVisualizer::createPathGeometry(
    const leoRouting::KShortestPath& path, int rank) const
{
    osg::ref_ptr<osg::Vec3dArray> vertices = new osg::Vec3dArray();
    vertices->reserve(path.nodeIds.size());
    for (int32_t nodeId : path.nodeIds)
        vertices->push_back(positionForNode(nodeId));

    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();
    geometry->setVertexArray(vertices);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINE_STRIP, 0, vertices->size()));

    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    colors->push_back(pathColor(rank));
    geometry->setColorArray(colors, osg::Array::BIND_OVERALL);

    osg::StateSet *stateSet = geometry->getOrCreateStateSet();
    const float width = std::max(2.0f, 6.0f - static_cast<float>(rank) * 0.35f);
    stateSet->setAttributeAndModes(new osg::LineWidth(width), osg::StateAttribute::ON);
    stateSet->setMode(GL_LIGHTING, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);
    stateSet->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateSet->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
    stateSet->setRenderBinDetails(100 + (pathCount - rank), "RenderBin");
    return geometry;
}

void LeoKPathOsgVisualizer::updateEndpointMarkers(int32_t sourceNodeId, int32_t destinationNodeId)
{
    selectedEndpointGeode->removeDrawables(0, selectedEndpointGeode->getNumDrawables());

    osg::ref_ptr<osg::Vec3dArray> source = new osg::Vec3dArray();
    source->push_back(positionForNode(sourceNodeId));
    selectedEndpointGeode->addDrawable(createPointGeometry(
        source, osg::Vec4(0.15f, 1.0f, 0.2f, 1.0f),
        static_cast<float>(par("selectedEndpointPointSize").doubleValue()), false));

    osg::ref_ptr<osg::Vec3dArray> destination = new osg::Vec3dArray();
    destination->push_back(positionForNode(destinationNodeId));
    selectedEndpointGeode->addDrawable(createPointGeometry(
        destination, osg::Vec4(1.0f, 0.15f, 0.15f, 1.0f),
        static_cast<float>(par("selectedEndpointPointSize").doubleValue()), false));
}

void LeoKPathOsgVisualizer::updateLegend(
    const leoRouting::KShortestPathGroup& group, omnetpp::simtime_t snapshotTime)
{
    const auto [sourceIndex, destinationIndex] = terminalPairs[pairIndex];
    const char *sourceLabel = getParentModule()->getSubmodule("userTerminal", sourceIndex)->par("label");
    const char *destinationLabel = getParentModule()->getSubmodule("userTerminal", destinationIndex)->par("label");

    std::ostringstream header;
    header << policyLabel << " | " << sourceLabel << " -> " << destinationLabel << "\n"
           << "Snapshot " << snapshotTime << " | " << group.paths.size() << "/" << pathCount
           << " ranked paths";
    headerText->setText(header.str());

    for (int rank = 0; rank < pathCount; ++rank) {
        std::ostringstream line;
        line << "Rank " << rank + 1 << "  ";
        if (rank < static_cast<int>(group.paths.size())) {
            const auto& path = group.paths[rank];
            line << std::fixed << std::setprecision(2) << path.rttMs << " ms RTT  |  "
                 << (path.nodeIds.empty() ? 0 : path.nodeIds.size() - 1) << " links";
        }
        else
            line << "unavailable";
        rankTexts[rank]->setText(line.str());
    }

    std::ostringstream status;
    status << policyLabel << ": " << sourceLabel << " to " << destinationLabel
           << ", " << group.paths.size() << "/" << pathCount << " paths";
    getDisplayString().setTagArg("t", 0, status.str().c_str());
}

osg::Vec3d LeoKPathOsgVisualizer::positionForNode(int32_t nodeId) const
{
    if (nodeId >= 0 && nodeId < numSatellites)
        return satellitePositions[nodeId];

    const int32_t groundIndex = nodeId - numSatellites;
    if (groundIndex >= 0 && groundIndex < numGroundStations)
        return groundStationPositions[groundIndex];

    const int32_t endpointStart = numSatellites + numGroundStations + 2 * numClients;
    const int32_t terminalIndex = nodeId - endpointStart;
    if (terminalIndex >= 0 && terminalIndex < numUserTerminals)
        return userTerminalPositions[terminalIndex];

    throw omnetpp::cRuntimeError("K-path node ID %d cannot be mapped by the visualization network", nodeId);
}

osg::Vec3d LeoKPathOsgVisualizer::geodeticToWorld(
    double longitudeDegrees, double latitudeDegrees, double altitudeMeters)
{
    constexpr double PI_VALUE = 3.14159265358979323846;
    constexpr double WGS84_A = 6378137.0;
    constexpr double WGS84_E2 = 6.69437999014e-3;
    const double longitude = longitudeDegrees * PI_VALUE / 180.0;
    const double latitude = latitudeDegrees * PI_VALUE / 180.0;
    const double sinLatitude = std::sin(latitude);
    const double cosLatitude = std::cos(latitude);
    const double radius = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLatitude * sinLatitude);

    return osg::Vec3d(
        (radius + altitudeMeters) * cosLatitude * std::cos(longitude),
        (radius + altitudeMeters) * cosLatitude * std::sin(longitude),
        (radius * (1.0 - WGS84_E2) + altitudeMeters) * sinLatitude);
}

osg::Vec4 LeoKPathOsgVisualizer::pathColor(int rank)
{
    static const osg::Vec4 colors[] = {
        osg::Vec4(0.00f, 0.90f, 0.46f, 1.0f),
        osg::Vec4(0.39f, 0.87f, 0.09f, 1.0f),
        osg::Vec4(0.68f, 0.92f, 0.00f, 1.0f),
        osg::Vec4(1.00f, 0.84f, 0.00f, 1.0f),
        osg::Vec4(1.00f, 0.67f, 0.00f, 1.0f),
        osg::Vec4(1.00f, 0.43f, 0.00f, 1.0f),
        osg::Vec4(1.00f, 0.24f, 0.00f, 1.0f),
        osg::Vec4(0.96f, 0.13f, 0.18f, 1.0f),
        osg::Vec4(0.72f, 0.11f, 0.11f, 1.0f),
        osg::Vec4(0.43f, 0.00f, 0.00f, 1.0f),
    };
    return colors[std::clamp(rank, 0, 9)];
}

#endif

} // namespace inet
