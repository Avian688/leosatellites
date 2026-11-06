#include "../../src/common/LeoReplayCheckpoint.h"
#include <functional>
#include <iostream>
#include <sstream>
using namespace inet::leoReplay;

int main()
{
    try {
        Checkpoint original;
        original.scale = -6;
        original.boundary = 170000000;
        original.nextUpdate = 170000001;
        original.nextAttachmentCheck = 180000000;
        original.lastAttachmentChange = 169980000;
        original.signature = "constellation and endpoint geometry";
        original.fixedDelays = {1234, 5678};
        original.groundLinks = {{0, 1, 1000}};
        original.terminals = {{2, -1, -1, 3210, 0, 0},
                              {-1, 3, 1, 0, 169980000, 170020000},
                              {-1, -1, -1, 0, 0, 0}};
        auto encode = [](const Checkpoint& state) {
            std::ostringstream out;
            writeCheckpoint(out, state);
            return out.str();
        };
        auto decode = [](const std::string& text) {
            std::istringstream in(text);
            return readCheckpoint(in, -6, 170000000, 100000, 15000000,
                                  "constellation and endpoint geometry", 2, 4, 1, 3);
        };
        auto restored = decode(encode(original));
        if (encode(restored) != encode(original) || restored.nextUpdate - restored.boundary != 1 ||
            restored.nextAttachmentCheck - restored.boundary != 10000000 ||
            restored.terminals[1].reconnect - restored.boundary != 20000 ||
            restored.terminals[1].start - restored.boundary != -20000)
            throw std::runtime_error("checkpoint lost state or original event phase");
        int rejected = 0;
        auto reject = [&](const std::string& text) {
            try { decode(text); }
            catch (const std::exception&) { ++rejected; return; }
            throw std::runtime_error("accepted invalid checkpoint");
        };
        auto bad = original; bad.boundary++; reject(encode(bad));
        bad = original; bad.scale = -12; reject(encode(bad));
        bad = original; bad.signature += " changed"; reject(encode(bad));
        bad = original; bad.nextUpdate = 169999999; reject(encode(bad));
        bad = original; bad.nextAttachmentCheck = 169000000; reject(encode(bad));
        bad = original; bad.fixedDelays[0] = -1; reject(encode(bad));
        bad = original; bad.groundLinks.push_back(bad.groundLinks[0]); reject(encode(bad));
        bad = original; bad.terminals[0].satellite = 4; reject(encode(bad));
        bad = original; bad.terminals[1].reconnect = 169999999; reject(encode(bad));
        bad = original; bad.terminals[1].satellite = 2; reject(encode(bad));
        bad = original; bad.terminals.pop_back(); reject(encode(bad));
        reject(encode(original).substr(0, encode(original).size() - 6));
        reject(encode(original) + "unexpected");
        bad = original; bad.lastAttachmentChange = 170000001; reject(encode(bad));
        std::cout << "PASS: checkpoint round trip, event offsets and " << rejected << " rejection cases\n";
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
