/*
 * LocalKCore.cpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <algorithm>
#include "networkit/GlobalState.hpp"
#include "networkit/graph/BFS.hpp"
#include "networkit/graph/GraphTools.hpp"
#include <networkit/centrality/CoreDecomposition.hpp>
#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/scd/LocalKCore.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>
#include <networkit/scd/SteinerKCore.hpp>

#include <tlx/container/ring_buffer.hpp>

#include <networkit/Globals.hpp>
#include <networkit/auxiliary/BucketPQ.hpp>
#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/SignalHandling.hpp>

namespace NetworKit {

LocalKCore::LocalKCore(const Graph &g, const std::string &logFile)
    : SelectiveCommunityDetector(g) {}

std::vector<node> getConnection(const Graph &g, const std::set<node> &s) {
    const node root = *s.begin();
    if (s.size() == 1)
        return {root};

    std::unordered_set<node> targets(std::next(s.begin()), s.end());
    size_t remaining = targets.size();
    std::unordered_map<node, node> parents;
    parents[root] = root;
    std::queue<node> q;
    q.push(root);

    while (!q.empty() && remaining) {
        node u = q.front();
        q.pop();
        g.forNeighborsOf(u, [&](node v) {
            if (parents.emplace(v, u).second) { // first visit
                remaining -= targets.contains(v);
                q.push(v);
            }
        });
    }

    if (remaining)
        throw std::invalid_argument("input set not connected");

    std::vector<node> connector{root};
    std::vector<bool> seen(g.upperNodeIdBound());
    for (auto it = std::next(s.begin()); it != s.end(); ++it) {
        for (node cur = *it; cur != root; cur = parents.at(cur))
            if (!seen[cur]) {
                seen[cur] = true;
                connector.push_back(cur);
            }
    }
    return connector;
};

template <typename G>
Graph makeSubgraph(const G &g, std::span<const node> nodes) {
    if constexpr (std::same_as<G, InducedSubgraphView>) {
        return GraphTools::subgraphFromNodes(g, nodes.begin(), nodes.end(), true);
    } else {
        InducedSubgraphView subg(g, true);
        subg.addNodes(nodes);
        return subg;
    }
}

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    // std::vector<node> nodes;
    // Graph subg = g->visit([&](const auto &g) { return makeSubgraph(g, nodes); });

    return {}; // FIXME
}

} // namespace NetworKit
