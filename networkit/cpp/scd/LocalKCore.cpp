/*
 * LocalKCore.cpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <networkit/scd/LocalKCore.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>

namespace NetworKit {

LocalKCore::LocalKCore(const Graph &g) : SelectiveCommunityDetector(g) {}

count hindex(const Graph &g, node u, const std::vector<count> &uppers);

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    INFO("calling LocalKCore::expandOneCommunity (not yet implemented)");

    // 1. create a spanning tree of s (BFS)
    // 2. initiate the frontier
    // 3. loop until convergence (or found bound, if maximal is not set to True):
    // 4. | add the frontier
    // 5. | if volume doubled, update lower bounds
    // 6. | update upper-bounds on all nodes
    std::set<node> output;
    return output;
}

// std::set<node> LocalKCore::expandOneCommunity(node s) {
//     std::set<node> output;
//     return output;
// }

} // namespace NetworKit
