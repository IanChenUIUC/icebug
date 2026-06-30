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
#include <networkit/scd/SteinerKCore.hpp>

#include <tlx/container/ring_buffer.hpp>

#include <algorithm>
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
    do {
        sum += buf[retval];
    } while (sum < retval && --retval);
    return retval;
}

InducedSubgraphView connectedSubgraph(const Graph &g, const std::set<node> &s) {
    InducedSubgraphView subg(g, {*s.begin()});
    count seen = 1;
    while (seen < s.size()) {
        auto nbr = subg.frontier();
        if (nbr.empty())
            throw std::invalid_argument("input set not connected");
        seen += std::count_if(nbr.begin(), nbr.end(), [&](node u) { return s.contains(u); });
        subg.addNodes(nbr);
    }
    return subg;
}

InducedSubgraphView componentOf(const Graph &g, node u) {
    InducedSubgraphView subg(g, {u});
    std::set<node> nbr;
    do {
        nbr = std::move(subg.frontier());
        subg.addNodes(nbr);
    } while (!nbr.empty());
    return subg;
}

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    assert(!s.empty());

    // 1. BFS until find connected subgraph containing s
    InducedSubgraphView subg = connectedSubgraph(*g, s);

    // 2. initiate all variables
    count lb = 0;
    std::vector<count> ub(g->upperNodeIdBound());
    subg.forNodes([&](node u) { ub[u] = g->degree(u); });

    count vol = s.size();
    count threshold = 0;

    std::vector<bool> inQueue(g->upperNodeIdBound());
    std::vector<bool> pruned(g->upperNodeIdBound());
    tlx::RingBuffer<node> queue(g->numberOfNodes());
    std::unordered_map<node, count> counts;

    auto compute_nbr_counts = [&](node u) {
        auto nbr = subg.neighborRange(u);
        counts[u] = std::count_if(nbr.begin(), nbr.end(), [&](node v) { return ub[v] >= ub[u]; });
    };

    auto remove_node = [&](node u) {
        subg.removeNode(u);
        pruned[u] = true;
    };

    auto push = [&](node u) {
        inQueue[u] = true;
        queue.push_back(u);
    };

    auto pop = [&]() {
        node u = queue.front();
        inQueue[u] = false;
        queue.pop_front();
        return u;
    };

    // 3. loop until convergence (or found bound, if maximal is not set to True):
    // | a) add the frontier
    // | b) update upper-bounds on all nodes
    // | c) if threshold passes: update lower bounds and prune
    std::vector<count> buf;
    bool frontierDone = false;
    subg.forNodes([&](node u) { push(u); });
    while (queue.size() && !frontierDone) {
        // store the nodes that will be get h-index propagated in this round
        count iters = queue.size();

        // a) add the frontier
        if (!frontierDone) {
            std::set<node> frontier = subg.frontier();
            frontierDone = frontier.empty();

            // add to the queue for the next phase
            vol += frontier.size();
            for (node u : frontier) {
                if (pruned[u])
                    continue;
                ub[u] = g->degree(u);
                subg.addNode(u);
                compute_nbr_counts(u);
                push(u);
            }
        }

        // b) update the upper-bounds of all nodes
        // TODO: convert to vector and parallelize, if there are enough nodes
        for (index i = 0; i < iters; ++i) {
            node u = pop();

            if (pruned[u])
                continue;

            // update u
            count kold = ub[u];
            count knew = ub[u] = hindex(*g, u, ub, buf);
            if (knew < lb) {
                remove_node(u);
            } else if (knew < kold) {
                // update nbr(u)
                compute_nbr_counts(u);
                subg.forNeighborsOf(u, [&](node v) {
                    if (knew < ub[v] && ub[v] <= kold && !inQueue[v] && --counts[v] < ub[v])
                        push(v);
                });
            }
        }

        // c) if threshold passes: update lower bounds and prune
        if (vol > threshold) {
            threshold = 2 * vol;

            auto ks = CoreDecomposition(subg, false, true);
            ks.run();
            subg.forNodes([&](node v) { lb = std::min(lb, static_cast<count>(ks.score(v))); });

            std::vector<node> toPrune;
            subg.forNodes([&](node v) {
                if (ub[v] < lb)
                    toPrune.push_back(v);
            });
            for (auto v : toPrune)
                remove_node(v);
        }
    }

    auto ks = CoreDecomposition(subg, false, true);
    ks.run();
    auto scores = ks.scores();
    std::vector<count> aa(scores.begin(), scores.end());
    auto steiner = SteinerKCore(subg, aa);
    return steiner.expandOneCommunity(s);
}

} // namespace NetworKit
