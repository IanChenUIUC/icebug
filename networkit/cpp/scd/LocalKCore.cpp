/*
 * LocalKCore.cpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <networkit/centrality/CoreDecomposition.hpp>
#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/scd/LocalKCore.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>

#include <queue>
#include <stdexcept>

namespace NetworKit {

LocalKCore::LocalKCore(const Graph &g) : SelectiveCommunityDetector(g) {}

count hindex(const Graph &g, node u, const std::vector<count> &uppers);

std::set<node> spanning(const Graph &g, const std::set<node> &s) {
    std::set<node> out;
    std::queue<node> q;
    for (node u : s)
        q.push(u);

    count seen = 0;
    while (!q.empty() && seen < s.size()) {
        node u = q.front();
        q.pop();

        if (s.contains(u))
            seen += 1;

        g.forEdgesOf(u, [&](node v) {
            if (!out.contains(v)) {
                q.push(v);
                out.insert(v);
            }
        });
    }

    if (seen < s.size())
        throw std::invalid_argument("input set not connected");

    return out;
}

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    INFO("calling LocalKCore::expandOneCommunity (not yet implemented)");

    // 1. BFS until find spanning subgraph of s
    InducedSubgraphView subg(*g, spanning(*g, s));

    // 2. initiate all variables
    std::vector<count> lower(g->upperNodeIdBound());
    std::vector<count> upper(g->upperNodeIdBound());
    subg.forNodes([&](node u) { upper[u] = subg.degree(u); });

    count vol = 2 * subg.numberOfEdges();
    count threshold = 0;

    // 3. loop until convergence (or found bound, if maximal is not set to True):
    // | a) add the boundary
    // | b) update upper-bounds on all nodes
    // | c) if threshold passes: update lower bounds and prune
    while (true) {
        std::set<node> startNodes = subg.getNodeSubset();

        for (node u : startNodes) {
            // TODO: maintain counts so that this is theoretically efficient
            // TODO: propagate h-index thingy (parallel)
        }

        std::set<node> frontier = subg.boundary(); // TODO: filter to remove seen
        subg.AddNodes(subg.boundary());
        vol = 2 * subg.numberOfEdges();
        if (vol > threshold) {
            auto coreness = CoreDecomposition(subg);
            coreness.run();
            subg.forNodes([&](node u) { lower[u] = static_cast<count>(coreness.score(u)); });
            threshold = 2 * vol;

            // TODO: prune nodes with upper bound < lower bound
        }

        // TODO: figure out how to check convergence
    }

    return subg.getNodeSubset();
}

// std::set<node> LocalKCore::expandOneCommunity(node s) {
//     std::set<node> output;
//     return output;
// }

} // namespace NetworKit
