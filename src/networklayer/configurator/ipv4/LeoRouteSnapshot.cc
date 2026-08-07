#include "LeoRouteSnapshot.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include <unistd.h>

namespace inet {
namespace leoRouting {

namespace {

uint64_t splitmix64(uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

uint64_t joinUint64(int32_t low, int32_t high)
{
    return static_cast<uint64_t>(static_cast<uint32_t>(low)) |
           (static_cast<uint64_t>(static_cast<uint32_t>(high)) << 32);
}

void appendUint64(std::vector<int32_t>& words, uint64_t value)
{
    words.push_back(static_cast<int32_t>(static_cast<uint32_t>(value)));
    words.push_back(static_cast<int32_t>(static_cast<uint32_t>(value >> 32)));
}

uint32_t decodeLittleEndianWord(const unsigned char *bytes)
{
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

void writeLittleEndianWord(std::ostream& output, int32_t word)
{
    const uint32_t value = static_cast<uint32_t>(word);
    const unsigned char bytes[4] = {
        static_cast<unsigned char>(value),
        static_cast<unsigned char>(value >> 8),
        static_cast<unsigned char>(value >> 16),
        static_cast<unsigned char>(value >> 24),
    };
    output.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
}

std::vector<int32_t> readWords(const std::filesystem::path& path, uint64_t& bytesRead)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
        throw std::runtime_error("Cannot open routing snapshot " + path.string());

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size <= 0)
        throw std::runtime_error("Routing snapshot is empty: " + path.string());
    if (size % 4 != 0)
        throw std::runtime_error("Routing snapshot is not 32-bit aligned: " + path.string());
    if (static_cast<uint64_t>(size) > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
        throw std::runtime_error("Routing snapshot is too large: " + path.string());

    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    input.seekg(0, std::ios::beg);
    input.read(reinterpret_cast<char *>(bytes.data()), size);
    if (input.gcount() != size || input.fail())
        throw std::runtime_error("Routing snapshot is truncated while reading: " + path.string());

    std::vector<int32_t> words(bytes.size() / 4);
    for (size_t i = 0; i < words.size(); ++i)
        words[i] = static_cast<int32_t>(decodeLittleEndianWord(bytes.data() + i * 4));
    bytesRead = bytes.size();
    return words;
}

std::string routeLabel(int32_t source, int32_t destination)
{
    return "(" + std::to_string(source) + "," + std::to_string(destination) + ")";
}

void validateDimensions(int32_t sourceCount, int32_t destinationCount)
{
    if (sourceCount <= 0 || destinationCount <= 0)
        throw std::runtime_error("Routing snapshot dimensions must be positive");
    const uint64_t entries = static_cast<uint64_t>(sourceCount) * static_cast<uint64_t>(destinationCount);
    if (entries > static_cast<uint64_t>(std::numeric_limits<size_t>::max() / sizeof(int32_t)))
        throw std::runtime_error("Routing snapshot dimensions are too large");
}

void validateRecord(const RouteRecord& record,
                    int32_t sourceCount,
                    int32_t destinationCount,
                    bool deleteAllowed,
                    const std::string& context)
{
    if (record.source < 0 || record.source >= sourceCount)
        throw std::runtime_error("Bad source node " + std::to_string(record.source) + " in " + context);
    if (record.destination < 0 || record.destination >= destinationCount)
        throw std::runtime_error("Bad destination node " + std::to_string(record.destination) + " in " + context);
    if (record.nextHop == DELETE_NEXT_HOP) {
        if (!deleteAllowed)
            throw std::runtime_error("DELETE is not valid in a base snapshot at route " +
                                     routeLabel(record.source, record.destination) + " in " + context);
    }
    else if (record.nextHop < 0 || record.nextHop >= destinationCount) {
        throw std::runtime_error("Bad stable next-hop node " + std::to_string(record.nextHop) +
                                 " at route " + routeLabel(record.source, record.destination) +
                                 " in " + context);
    }
}

} // namespace

uint64_t routeContribution(int32_t source, int32_t destination, int32_t nextHop)
{
    uint64_t value = splitmix64(static_cast<uint32_t>(source));
    value ^= splitmix64(static_cast<uint32_t>(destination) + 0x100000001b3ULL);
    value ^= splitmix64(static_cast<uint32_t>(nextHop) + 0x517cc1b727220a95ULL);
    return splitmix64(value);
}

uint64_t computePayloadHash(const std::vector<RouteRecord>& records)
{
    uint64_t hash = 1469598103934665603ULL;
    auto consume = [&hash](int32_t word) {
        uint32_t value = static_cast<uint32_t>(word);
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= static_cast<unsigned char>(value >> (byte * 8));
            hash *= 1099511628211ULL;
        }
    };
    for (const RouteRecord& record : records) {
        consume(record.source);
        consume(record.destination);
        consume(record.nextHop);
    }
    return hash;
}

StableRouteState::StableRouteState(int32_t sourceCount, int32_t destinationCount)
{
    reset(sourceCount, destinationCount);
}

void StableRouteState::reset(int32_t sourceCount, int32_t destinationCount)
{
    validateDimensions(sourceCount, destinationCount);
    sourceCount_ = sourceCount;
    destinationCount_ = destinationCount;
    routes.assign(static_cast<size_t>(sourceCount) * destinationCount, DELETE_NEXT_HOP);
    routeCount_ = 0;
    stateHash_ = 0;
    hasSequenceMetadata_ = false;
    sequence_ = -1;
    baseSequence_ = -1;
    timestampMicros_ = 0;
    baseStateHash_ = 0;
}

size_t StableRouteState::routeIndex(int32_t source, int32_t destination) const
{
    if (source < 0 || source >= sourceCount_ || destination < 0 || destination >= destinationCount_)
        throw std::out_of_range("Route index " + routeLabel(source, destination) + " is out of range");
    return static_cast<size_t>(source) * destinationCount_ + destination;
}

int32_t StableRouteState::get(int32_t source, int32_t destination) const
{
    return routes.at(routeIndex(source, destination));
}

void StableRouteState::setRouteUnchecked(int32_t source, int32_t destination, int32_t nextHop)
{
    int32_t& current = routes[routeIndex(source, destination)];
    if (current == nextHop)
        return;
    if (current != DELETE_NEXT_HOP) {
        stateHash_ ^= routeContribution(source, destination, current);
        routeCount_--;
    }
    current = nextHop;
    if (current != DELETE_NEXT_HOP) {
        stateHash_ ^= routeContribution(source, destination, current);
        routeCount_++;
    }
}

void StableRouteState::setRoute(int32_t source, int32_t destination, int32_t nextHop)
{
    validateRecord({source, destination, nextHop}, sourceCount_, destinationCount_, true, "route state");
    setRouteUnchecked(source, destination, nextHop);
}

std::vector<RouteRecord> StableRouteState::effectiveRecords() const
{
    std::vector<RouteRecord> result;
    result.reserve(routeCount_);
    for (int32_t source = 0; source < sourceCount_; ++source) {
        for (int32_t destination = 0; destination < destinationCount_; ++destination) {
            const int32_t nextHop = routes[static_cast<size_t>(source) * destinationCount_ + destination];
            if (nextHop != DELETE_NEXT_HOP)
                result.push_back({source, destination, nextHop});
        }
    }
    return result;
}

void StableRouteState::setSnapshotMetadata(const SnapshotHeader& header)
{
    hasSequenceMetadata_ = true;
    sequence_ = header.sequence;
    baseSequence_ = header.baseSequence;
    timestampMicros_ = header.timestampMicros;
    baseStateHash_ = header.baseStateHash;
}

StableRouteState StableRouteState::fromFullSnapshot(const ParsedSnapshot& snapshot,
                                                    int32_t expectedSourceCount,
                                                    int32_t expectedDestinationCount,
                                                    FullDecodeStats *stats)
{
    StableRouteState result(expectedSourceCount, expectedDestinationCount);
    const bool isV3 = snapshot.format == SnapshotFormat::V3;
    if (isV3) {
        const SnapshotHeader& header = snapshot.header;
        if (header.kind != SnapshotKind::Base)
            throw std::runtime_error("A delta snapshot cannot be decoded as a full/base snapshot");
        if (header.sequence != 0 || header.baseSequence != 0 || header.previousSequence != -1)
            throw std::runtime_error("A v3 base snapshot must have sequence=0, baseSequence=0, previousSequence=-1");
        if (header.sourceCount != expectedSourceCount || header.destinationCount != expectedDestinationCount)
            throw std::runtime_error("Base snapshot dimensions do not match the current topology");
        if (header.previousRouteCount != 0 || header.previousStateHash != 0)
            throw std::runtime_error("A v3 base snapshot must not declare preceding state");
    }

    FullDecodeStats localStats;
    localStats.inputRecords = snapshot.records.size();
    for (const RouteRecord& record : snapshot.records) {
        validateRecord(record, expectedSourceCount, expectedDestinationCount, false,
                       isV3 ? "v3 base snapshot" : "v2 full snapshot");
        const int32_t oldNextHop = result.get(record.source, record.destination);
        if (oldNextHop != DELETE_NEXT_HOP) {
            localStats.duplicateRecords++;
            if (isV3)
                throw std::runtime_error("Duplicate route " + routeLabel(record.source, record.destination) +
                                         " in canonical v3 base snapshot");
        }
        // Repeated v2 records intentionally retain the final value.
        result.setRouteUnchecked(record.source, record.destination, record.nextHop);
    }
    localStats.effectiveRoutes = result.routeCount();

    if (isV3) {
        const SnapshotHeader& header = snapshot.header;
        if (result.routeCount() != header.resultRouteCount)
            throw std::runtime_error("Base snapshot effective route count does not match its header");
        if (result.stateHash() != header.resultStateHash)
            throw std::runtime_error("Base snapshot state hash does not match its header");
        if (header.baseStateHash != header.resultStateHash)
            throw std::runtime_error("Base snapshot base-state hash does not match its result-state hash");
        result.setSnapshotMetadata(header);
    }

    if (stats != nullptr)
        *stats = localStats;
    return result;
}

DeltaPreview StableRouteState::validateDelta(const ParsedSnapshot& snapshot) const
{
    if (snapshot.format != SnapshotFormat::V3 || snapshot.header.kind != SnapshotKind::Delta)
        throw std::runtime_error("Expected a v3 delta snapshot");
    if (!hasSequenceMetadata_)
        throw std::runtime_error("Cannot apply a delta snapshot without its v3 base snapshot");

    const SnapshotHeader& header = snapshot.header;
    if (header.sourceCount != sourceCount_ || header.destinationCount != destinationCount_)
        throw std::runtime_error("Delta snapshot dimensions do not match the loaded base");
    if (header.sequence != sequence_ + 1 || header.previousSequence != sequence_)
        throw std::runtime_error("Out-of-order delta: loaded sequence " + std::to_string(sequence_) +
                                 ", delta sequence " + std::to_string(header.sequence) +
                                 ", declared predecessor " + std::to_string(header.previousSequence));
    if (header.baseSequence != baseSequence_ || header.baseStateHash != baseStateHash_)
        throw std::runtime_error("Delta snapshot does not belong to the loaded base snapshot");
    if (header.timestampMicros <= timestampMicros_)
        throw std::runtime_error("Delta snapshot timestamp is not later than its predecessor");
    if (header.previousRouteCount != routeCount_ || header.previousStateHash != stateHash_)
        throw std::runtime_error("Delta snapshot preceding-state metadata does not match the loaded state");

    uint64_t predictedCount = routeCount_;
    uint64_t predictedHash = stateHash_;
    std::unordered_set<uint64_t> changedKeys;
    changedKeys.reserve(snapshot.records.size() * 2 + 1);
    for (const RouteRecord& record : snapshot.records) {
        validateRecord(record, sourceCount_, destinationCount_, true, "v3 delta snapshot");
        const uint64_t key = static_cast<uint64_t>(static_cast<uint32_t>(record.source)) << 32 |
                             static_cast<uint32_t>(record.destination);
        if (!changedKeys.insert(key).second)
            throw std::runtime_error("Duplicate delta operation for route " +
                                     routeLabel(record.source, record.destination));

        const int32_t oldNextHop = get(record.source, record.destination);
        if (record.nextHop == DELETE_NEXT_HOP) {
            if (oldNextHop == DELETE_NEXT_HOP)
                throw std::runtime_error("DELETE targets a missing route " +
                                         routeLabel(record.source, record.destination));
            predictedHash ^= routeContribution(record.source, record.destination, oldNextHop);
            predictedCount--;
        }
        else {
            if (oldNextHop == record.nextHop)
                throw std::runtime_error("Delta contains an unchanged SET for route " +
                                         routeLabel(record.source, record.destination));
            if (oldNextHop != DELETE_NEXT_HOP)
                predictedHash ^= routeContribution(record.source, record.destination, oldNextHop);
            else
                predictedCount++;
            predictedHash ^= routeContribution(record.source, record.destination, record.nextHop);
        }
    }

    if (predictedCount != header.resultRouteCount)
        throw std::runtime_error("Delta result route count does not match its header");
    if (predictedHash != header.resultStateHash)
        throw std::runtime_error("Delta result state hash does not match its header");
    return {predictedCount, predictedHash};
}

void StableRouteState::applyValidatedDelta(const ParsedSnapshot& snapshot, const DeltaPreview& preview)
{
    for (const RouteRecord& record : snapshot.records)
        setRouteUnchecked(record.source, record.destination, record.nextHop);
    if (routeCount_ != preview.resultRouteCount || stateHash_ != preview.resultStateHash)
        throw std::logic_error("Validated delta produced an unexpected state");
    setSnapshotMetadata(snapshot.header);
}

void NextHopInterfaceTable::reset(int32_t trackedSourceCount, int32_t nodeCount)
{
    if (trackedSourceCount < 0 || nodeCount <= 0 || trackedSourceCount > nodeCount)
        throw std::invalid_argument("Invalid next-hop interface table dimensions");
    trackedSourceCount_ = trackedSourceCount;
    nodeCount_ = nodeCount;
    interfaces.assign(static_cast<size_t>(nodeCount) * nodeCount, -1);
    dirtySources_.assign(trackedSourceCount, 1);
}

size_t NextHopInterfaceTable::interfaceIndex(int32_t source, int32_t nextHop) const
{
    if (source < 0 || source >= nodeCount_ || nextHop < 0 || nextHop >= nodeCount_)
        throw std::out_of_range("Next-hop interface index is out of range");
    return static_cast<size_t>(source) * nodeCount_ + nextHop;
}

bool NextHopInterfaceTable::set(int32_t source, int32_t nextHop, int32_t interfaceId)
{
    int32_t& current = interfaces.at(interfaceIndex(source, nextHop));
    if (current == interfaceId)
        return false;
    current = interfaceId;
    if (source < trackedSourceCount_)
        dirtySources_[source] = 1;
    return true;
}

int32_t NextHopInterfaceTable::get(int32_t source, int32_t nextHop) const
{
    return interfaces.at(interfaceIndex(source, nextHop));
}

bool NextHopInterfaceTable::isDirty(int32_t source) const
{
    if (source < 0 || source >= trackedSourceCount_)
        return false;
    return dirtySources_[source] != 0;
}

std::vector<int32_t> NextHopInterfaceTable::dirtySources() const
{
    std::vector<int32_t> result;
    for (int32_t source = 0; source < trackedSourceCount_; ++source) {
        if (dirtySources_[source])
            result.push_back(source);
    }
    return result;
}

void NextHopInterfaceTable::clearDirty(int32_t source)
{
    if (source >= 0 && source < trackedSourceCount_)
        dirtySources_[source] = 0;
}

void NextHopInterfaceTable::clearAllDirty()
{
    std::fill(dirtySources_.begin(), dirtySources_.end(), 0);
}

void NextHopInterfaceTable::markAllDirty()
{
    std::fill(dirtySources_.begin(), dirtySources_.end(), 1);
}

int64_t parseTimestampMicros(const std::string& timestamp)
{
    if (timestamp.empty())
        throw std::runtime_error("Empty routing snapshot timestamp");
    const size_t dot = timestamp.find('.');
    if (dot != std::string::npos && timestamp.find('.', dot + 1) != std::string::npos)
        throw std::runtime_error("Malformed routing snapshot timestamp: " + timestamp);

    const std::string secondsPart = dot == std::string::npos ? timestamp : timestamp.substr(0, dot);
    std::string fraction = dot == std::string::npos ? "" : timestamp.substr(dot + 1);
    if (secondsPart.empty() || !std::all_of(secondsPart.begin(), secondsPart.end(), ::isdigit) ||
        !std::all_of(fraction.begin(), fraction.end(), ::isdigit))
        throw std::runtime_error("Malformed routing snapshot timestamp: " + timestamp);
    if (fraction.size() > 6) {
        if (!std::all_of(fraction.begin() + 6, fraction.end(), [](char value) { return value == '0'; }))
            throw std::runtime_error("Routing snapshot timestamp is more precise than one microsecond: " + timestamp);
        fraction.resize(6);
    }
    while (fraction.size() < 6)
        fraction.push_back('0');

    uint64_t seconds = 0;
    for (char digit : secondsPart) {
        if (seconds > (static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) / 1000000ULL - 9) / 10)
            throw std::runtime_error("Routing snapshot timestamp is too large: " + timestamp);
        seconds = seconds * 10 + static_cast<unsigned>(digit - '0');
    }
    uint64_t micros = 0;
    for (char digit : fraction)
        micros = micros * 10 + static_cast<unsigned>(digit - '0');
    const uint64_t result = seconds * 1000000ULL + micros;
    if (result > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        throw std::runtime_error("Routing snapshot timestamp is too large: " + timestamp);
    return static_cast<int64_t>(result);
}

int64_t timestampMicrosFromPath(const std::filesystem::path& path)
{
    if (path.extension() != ".bin")
        throw std::runtime_error("Routing snapshot does not have a .bin extension: " + path.string());
    return parseTimestampMicros(path.stem().string());
}

std::vector<std::filesystem::path> listSnapshotFilesNumeric(const std::filesystem::path& directory)
{
    if (!std::filesystem::is_directory(directory))
        throw std::runtime_error("Routing snapshot directory does not exist: " + directory.string());
    std::vector<std::pair<int64_t, std::filesystem::path>> timestamped;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".bin")
            timestamped.emplace_back(timestampMicrosFromPath(entry.path()), entry.path());
    }
    std::sort(timestamped.begin(), timestamped.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first)
            return left.first < right.first;
        return left.second.filename().string() < right.second.filename().string();
    });
    for (size_t index = 1; index < timestamped.size(); ++index) {
        if (timestamped[index - 1].first == timestamped[index].first)
            throw std::runtime_error("Duplicate numeric routing timestamp: " +
                                     timestamped[index - 1].second.string() + " and " +
                                     timestamped[index].second.string());
    }
    std::vector<std::filesystem::path> result;
    result.reserve(timestamped.size());
    for (auto& [timestamp, path] : timestamped)
        result.push_back(std::move(path));
    return result;
}

ParsedSnapshot readSnapshot(const std::filesystem::path& path)
{
    ParsedSnapshot result;
    std::vector<int32_t> words = readWords(path, result.bytesRead);
    if (words.size() < 2)
        throw std::runtime_error("Routing snapshot header is truncated: " + path.string());

    size_t recordOffset = 0;
    if (words[0] == V2_ROUTE_FILE_MAGIC) {
        if (words[1] != V2_ROUTE_FILE_VERSION)
            throw std::runtime_error("Unsupported LEO2 routing snapshot version " +
                                     std::to_string(words[1]) + " in " + path.string());
        result.format = SnapshotFormat::V2Full;
        recordOffset = 2;
    }
    else if (words[0] == V3_ROUTE_FILE_MAGIC) {
        if (words[1] != V3_ROUTE_FILE_VERSION)
            throw std::runtime_error("Unsupported LEO3 routing snapshot version " +
                                     std::to_string(words[1]) + " in " + path.string());
        if (words.size() < V3_HEADER_WORDS)
            throw std::runtime_error("LEO3 routing snapshot header is truncated: " + path.string());
        if (words[2] != V3_HEADER_WORDS)
            throw std::runtime_error("Unsupported LEO3 header length " + std::to_string(words[2]) +
                                     " in " + path.string());
        if (words[3] != static_cast<int32_t>(SnapshotKind::Base) &&
            words[3] != static_cast<int32_t>(SnapshotKind::Delta))
            throw std::runtime_error("Invalid LEO3 snapshot kind in " + path.string());

        result.format = SnapshotFormat::V3;
        SnapshotHeader& header = result.header;
        header.kind = static_cast<SnapshotKind>(words[3]);
        header.sequence = words[4];
        header.baseSequence = words[5];
        header.previousSequence = words[6];
        header.sourceCount = words[7];
        header.destinationCount = words[8];
        const int32_t recordCount = words[9];
        if (recordCount < 0)
            throw std::runtime_error("Negative LEO3 record count in " + path.string());
        header.timestampMicros = static_cast<int64_t>(joinUint64(words[10], words[11]));
        header.previousRouteCount = joinUint64(words[12], words[13]);
        header.resultRouteCount = joinUint64(words[14], words[15]);
        header.previousStateHash = joinUint64(words[16], words[17]);
        header.resultStateHash = joinUint64(words[18], words[19]);
        header.baseStateHash = joinUint64(words[20], words[21]);
        header.payloadHash = joinUint64(words[22], words[23]);
        validateDimensions(header.sourceCount, header.destinationCount);
        const uint64_t expectedWords = static_cast<uint64_t>(V3_HEADER_WORDS) +
                                       static_cast<uint64_t>(recordCount) * 3;
        if (expectedWords != words.size())
            throw std::runtime_error("LEO3 record count/file length mismatch in " + path.string());
        if (header.timestampMicros != timestampMicrosFromPath(path))
            throw std::runtime_error("LEO3 header timestamp does not match filename " + path.filename().string());
        recordOffset = V3_HEADER_WORDS;
    }
    else {
        throw std::runtime_error("Unknown routing snapshot magic in " + path.string());
    }

    if ((words.size() - recordOffset) % 3 != 0)
        throw std::runtime_error("Routing snapshot contains an incomplete route record: " + path.string());
    result.records.reserve((words.size() - recordOffset) / 3);
    for (size_t offset = recordOffset; offset < words.size(); offset += 3)
        result.records.push_back({words[offset], words[offset + 1], words[offset + 2]});

    if (result.format == SnapshotFormat::V3) {
        if (computePayloadHash(result.records) != result.header.payloadHash)
            throw std::runtime_error("LEO3 payload checksum mismatch in " + path.string());
        const bool deleteAllowed = result.header.kind == SnapshotKind::Delta;
        for (const RouteRecord& record : result.records)
            validateRecord(record, result.header.sourceCount, result.header.destinationCount,
                           deleteAllowed, path.string());
    }
    return result;
}

void writeV3SnapshotAtomic(const std::filesystem::path& path,
                           SnapshotHeader header,
                           const std::vector<RouteRecord>& records,
                           bool allowOverwrite)
{
    validateDimensions(header.sourceCount, header.destinationCount);
    if (records.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        throw std::runtime_error("Too many records for a LEO3 routing snapshot");
    for (const RouteRecord& record : records)
        validateRecord(record, header.sourceCount, header.destinationCount,
                       header.kind == SnapshotKind::Delta, path.string());
    header.payloadHash = computePayloadHash(records);

    std::vector<int32_t> headerWords;
    headerWords.reserve(V3_HEADER_WORDS);
    headerWords.push_back(V3_ROUTE_FILE_MAGIC);
    headerWords.push_back(V3_ROUTE_FILE_VERSION);
    headerWords.push_back(V3_HEADER_WORDS);
    headerWords.push_back(static_cast<int32_t>(header.kind));
    headerWords.push_back(header.sequence);
    headerWords.push_back(header.baseSequence);
    headerWords.push_back(header.previousSequence);
    headerWords.push_back(header.sourceCount);
    headerWords.push_back(header.destinationCount);
    headerWords.push_back(static_cast<int32_t>(records.size()));
    appendUint64(headerWords, static_cast<uint64_t>(header.timestampMicros));
    appendUint64(headerWords, header.previousRouteCount);
    appendUint64(headerWords, header.resultRouteCount);
    appendUint64(headerWords, header.previousStateHash);
    appendUint64(headerWords, header.resultStateHash);
    appendUint64(headerWords, header.baseStateHash);
    appendUint64(headerWords, header.payloadHash);
    if (headerWords.size() != V3_HEADER_WORDS)
        throw std::logic_error("LEO3 header size implementation error");

    std::filesystem::create_directories(path.parent_path());
    if (!allowOverwrite && std::filesystem::exists(path))
        throw std::runtime_error("Refusing to overwrite existing routing snapshot " + path.string());
    const std::filesystem::path temporary = path.string() + ".tmp." + std::to_string(getpid());
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
            throw std::runtime_error("Cannot create temporary routing snapshot " + temporary.string());
        for (int32_t word : headerWords)
            writeLittleEndianWord(output, word);
        for (const RouteRecord& record : records) {
            writeLittleEndianWord(output, record.source);
            writeLittleEndianWord(output, record.destination);
            writeLittleEndianWord(output, record.nextHop);
        }
        output.flush();
        if (!output.good()) {
            output.close();
            std::filesystem::remove(temporary);
            throw std::runtime_error("Failed while writing temporary routing snapshot " + temporary.string());
        }
    }
    std::error_code renameError;
    std::filesystem::rename(temporary, path, renameError);
    if (renameError) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot atomically publish routing snapshot " + path.string() +
                                 ": " + renameError.message());
    }
}

SnapshotHeader makeBaseHeader(const StableRouteState& state, int32_t sequence, int64_t timestampMicros)
{
    if (sequence != 0)
        throw std::invalid_argument("The first/base routing snapshot must use sequence 0");
    SnapshotHeader header;
    header.kind = SnapshotKind::Base;
    header.sequence = sequence;
    header.baseSequence = sequence;
    header.previousSequence = -1;
    header.sourceCount = state.sourceCount();
    header.destinationCount = state.destinationCount();
    header.timestampMicros = timestampMicros;
    header.resultRouteCount = state.routeCount();
    header.resultStateHash = state.stateHash();
    header.baseStateHash = state.stateHash();
    return header;
}

SnapshotHeader makeDeltaHeader(const StableRouteState& previousState,
                               const StableRouteState& resultState,
                               int32_t sequence,
                               int64_t timestampMicros)
{
    if (!previousState.hasSequenceMetadata())
        throw std::invalid_argument("Previous route state is missing base/sequence metadata");
    if (sequence != previousState.sequence() + 1)
        throw std::invalid_argument("Delta sequence is not the successor of the previous route state");
    if (previousState.sourceCount() != resultState.sourceCount() ||
        previousState.destinationCount() != resultState.destinationCount())
        throw std::invalid_argument("Cannot diff routing states with different dimensions");
    SnapshotHeader header;
    header.kind = SnapshotKind::Delta;
    header.sequence = sequence;
    header.baseSequence = previousState.baseSequence();
    header.previousSequence = previousState.sequence();
    header.sourceCount = resultState.sourceCount();
    header.destinationCount = resultState.destinationCount();
    header.timestampMicros = timestampMicros;
    header.previousRouteCount = previousState.routeCount();
    header.resultRouteCount = resultState.routeCount();
    header.previousStateHash = previousState.stateHash();
    header.resultStateHash = resultState.stateHash();
    header.baseStateHash = previousState.baseStateHash();
    return header;
}

std::vector<RouteRecord> diffRoutes(const StableRouteState& previousState,
                                    const StableRouteState& resultState)
{
    if (previousState.sourceCount() != resultState.sourceCount() ||
        previousState.destinationCount() != resultState.destinationCount())
        throw std::invalid_argument("Cannot diff routing states with different dimensions");
    std::vector<RouteRecord> changes;
    const std::vector<int32_t>& previous = previousState.rawRoutes();
    const std::vector<int32_t>& result = resultState.rawRoutes();
    for (size_t index = 0; index < previous.size(); ++index) {
        if (previous[index] == result[index])
            continue;
        const int32_t source = static_cast<int32_t>(index / previousState.destinationCount());
        const int32_t destination = static_cast<int32_t>(index % previousState.destinationCount());
        changes.push_back({source, destination, result[index]});
    }
    return changes;
}

int resolveRoute(const StableRouteState& routes,
                 const NextHopInterfaceTable& interfaces,
                 int32_t source,
                 int32_t destination)
{
    const int32_t nextHop = routes.get(source, destination);
    if (nextHop == DELETE_NEXT_HOP)
        return 0;
    const int32_t interfaceId = interfaces.get(source, nextHop);
    if (interfaceId <= 0)
        throw std::runtime_error("Cannot resolve current interface for stable route " +
                                 routeLabel(source, destination) + " via next-hop node " +
                                 std::to_string(nextHop));
    return interfaceId;
}

std::vector<int> resolveSourceRow(const StableRouteState& routes,
                                  const NextHopInterfaceTable& interfaces,
                                  int32_t source,
                                  size_t liveDestinationCount)
{
    if (liveDestinationCount < static_cast<size_t>(routes.destinationCount()))
        throw std::invalid_argument("Live route row is smaller than the stable route state");
    std::vector<int> result(liveDestinationCount, 0);
    for (int32_t destination = 0; destination < routes.destinationCount(); ++destination)
        result[destination] = resolveRoute(routes, interfaces, source, destination);
    return result;
}

} // namespace leoRouting
} // namespace inet
