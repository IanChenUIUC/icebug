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

#include <tlx/container/ring_buffer.hpp>

#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace NetworKit {

LocalKCore::LocalKCore(const Graph &g) : SelectiveCommunityDetector(g) {}

count hindex(const Graph &g, node u, const std::vector<count> &uppers, std::vector<count> &buf) {
    if (g.degree(u) == 0)
        return 0;

    buf.resize(0);
    buf.resize(g.degree(u) + 1); // zero-out

    g.forNeighborsOf(u, [&](node v) { ++buf[std::min(uppers[v], g.degree(u))]; });
    count sum = 0, retval = g.degree(u);
    while (sum < retval)
        sum += buf[retval--];
    return retval;
}

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
    // 1. BFS until find spanning subgraph of s
    InducedSubgraphView subg(*g, spanning(*g, s));

    // 2. initiate all variables
    std::vector<count> lowers(g->upperNodeIdBound());
    std::vector<count> uppers(g->upperNodeIdBound());
    subg.forNodes([&](node u) { uppers[u] = subg.degree(u); });

    count vol = 2 * subg.numberOfEdges();
    count threshold = 0;

    std::vector<bool> inQueue(g->upperNodeIdBound());
    std::vector<bool> pruned(g->upperNodeIdBound());
    tlx::RingBuffer<node> queue(g->numberOfNodes());
    std::unordered_map<node, count> counts;

    auto compute_nbr_counts = [&](node u) {
        auto nbr = subg.neighborRange(u);
        counts[u] =
            std::count_if(nbr.begin(), nbr.end(), [&](node v) { return uppers[v] >= uppers[u]; });
    };

    auto remove_node = [&](node u) {
        subg.removeNode(u);
        pruned[u] = true;
    };

    subg.forNodes([&](node u) {
        compute_nbr_counts(u);
        if (counts[u] < uppers[u])
            queue.push_back(u);
    });

    // 3. loop until convergence (or found bound, if maximal is not set to True):
    // | a) add the frontier
    // | b) update upper-bounds on all nodes
    // | c) if threshold passes: update lower bounds and prune
    std::vector<count> buf;

    bool frontierDone = false;
    while (queue.size() && !frontierDone) {
        // store the nodes that will be get h-index propagated in this round
        count iters = queue.size();

        // a) add the frontier
        if (!frontierDone) {
            std::set<node> frontier = subg.frontier();
            std::erase_if(frontier, [&](node u) { return pruned[u]; });
            subg.addNodes(frontier);
            frontierDone = frontier.empty();

            // add to the queue for the next phase
            for (node u : frontier)
                queue.push_back(u);
        }

        // b) update the upper-bounds of all nodes
        // TODO: convert to vector and parallelize, if there are enough nodes
        for (index i = 0; i < iters; ++i) {
            node u = queue.front();
            queue.pop_front();

            // update u
            count kold = uppers[u];
            count knew = uppers[u] = hindex(*g, u, uppers, buf);
            if (knew < lowers[*s.begin()])
                remove_node(u);
            else if (knew <= kold)
                continue;

            // update nbr(u)
            compute_nbr_counts(u);
            subg.forNeighborsOf(u, [&](node v) {
                if (knew < uppers[v] && uppers[v] <= kold && !inQueue[v]
                    && --counts[v] < uppers[v]) {
                    inQueue[v] = true;
                    queue.push_back(v);
                }
            });
        }

        // c) if threshold passes: update lower bounds and prune
        vol = 2 * subg.numberOfEdges();
        if (vol > threshold) {
            threshold = 2 * vol;

            auto coreness = CoreDecomposition(subg);
            coreness.run();
            subg.forNodes([&](node u) { lowers[u] = static_cast<count>(coreness.score(u)); });

            std::set<node> toPrune;
            subg.forNodes([&](node u) {
                if (uppers[u] < lowers[*s.begin()])
                    remove_node(u);
            });
        }
    }

    return subg.getNodeSubset();
}

} // namespace NetworKit
