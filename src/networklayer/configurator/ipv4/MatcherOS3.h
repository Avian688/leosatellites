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

#ifndef OS3_NETWORKLAYER_CONFIGURATOR_IPV4_MATCHEROS3_H_
#define OS3_NETWORKLAYER_CONFIGURATOR_IPV4_MATCHEROS3_H_
#include <inet/common/PatternMatcher.h>
namespace inet {
/**
 * This method was copied from INET. Due to OS3 linking errors OS3 could not use this class within the custom SatelliteNetworkConfigurator
 * model. Therefore the class was re-implemented for use. The class simply checks to see if two patterns match, such as if two
 * interfaces match.
 */
class MatcherOS3
    {
      protected:
        bool matchesany = false;
        std::vector<inet::PatternMatcher *> matchers;    // TODO replace with a MatchExpression once it becomes available in OMNeT++

      public:
        MatcherOS3(const char *pattern);
        ~MatcherOS3();

        bool matches(const char *s);
        bool matchesAny() { return matchesany; }
        bool isEmpty(const char *s) {return !s || !s[0]; }
    };
}
#endif /* OS3_NETWORKLAYER_CONFIGURATOR_IPV4_MATCHEROS3_H_ */
