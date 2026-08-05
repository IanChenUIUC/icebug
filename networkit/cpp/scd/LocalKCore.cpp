/*
 * LocalKCore.cpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <algorithm>
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
    // note: cannot use MultiTargetBFS as it does not store predecessors

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

InducedSubgraphView makeSubgraph(const Graph &g, std::span<const node> nodes) {
    return g->visit([&](const auto &g) {
        using T = std::decay_t<decltype(g)>;

        if constexpr (std::is_same_v<T, InducedSubgraphView>) {
            assert(false); // FIXME
            return g;
        } else if constexpr (std::is_same_v<T, GraphW> || std::is_same_v<T, GraphR>) {
            auto subg = InducedSubgraphView(g, true);
            subg.addNodes(nodes);
            return subg;
        }
    });
}

struct data_t {
    count upper;   // upper bound on coreness (*)
    count support; // number of nbrs with >= upper
    count nbrs;    // number of nbrs that have been considered
};

count hindex(const Graph &g, node u, const std::unordered_map<node, data_t> &candidates,
             std::vector<count> &buf) {
    if (g.degree(u) == 0)
        return 0;

    buf.resize(0);
    buf.resize(g.degree(u) + 1); // zero-out

    g.forNeighborsOf(u, [&](node v) {
        if (!candidates.contains(v))
            return;
        ++buf[std::min(candidates.at(v).upper, g.degree(u))];
    });
    count sum = 0, retval = g.degree(u);
    do {
        sum += buf[retval];
    } while (sum < retval && --retval);
    return retval;
}

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    // perf: contains() and then at() is redundant

    std::unordered_map<node, data_t> candidates;
    std::vector<bool> removed(g->upperNodeIdBound()); // whether its been pruned out or not
    count lower_bound = 0;                            // lower bound on coreness of s
    count potential, potential_old = 0;

    // store the priority queue based on largest incidence
    Aux::BucketPQ pq(g->numberOfNodes(), 0, static_cast<int64_t>(g->numberOfNodes()) - 1);

    auto init_data = [&](node u) -> data_t {
        count support = 0, nbrs = 0;
        g->forNeighborsOf(u, [&](node v) {
            count other_ub = candidates.contains(v) ? candidates.at(v).upper : g->degree(v);
            support += other_ub >= g->degree(u);
            nbrs += candidates.contains(v) || removed[v];
        });
        return data_t{.upper = g->degree(u), .support = support, .nbrs = nbrs};
    };

    auto add_to_pq = [&](node v) -> void {
        removed[v] = removed[v] || g->degree(v) < lower_bound;
        if (removed[v] || candidates.contains(v))
            return;

        if (pq.contains(v)) {
            pq.changeKey(pq.getKey(v) + 1, v);
        } else {
            int64_t incidence = 0;
            g->forNeighborsOf(v, [&](node w) { incidence += candidates.contains(w); });
            pq.insert(incidence, v);
        }
    };

    // queue of nodes that need to be tightened
    // only contain nodes that have all neighbors considered
    tlx::RingBuffer<node> triggerUpper(g->numberOfNodes());
    std::vector<bool> inQueue(g->upperNodeIdBound());
    for (node u : getConnection(*g, s))
        candidates[u] = data_t{};
    for (auto [u, _] : candidates) {
        candidates[u] = init_data(u);
        g->forNeighborsOf(u, add_to_pq);
    }

    // iterate in this order until stop
    std::vector<count> buf;
    while (!pq.empty()) {
        node u = pq.extractMax().second;
        auto u_data = candidates[u] = init_data(u);
        potential += g->degree(u) * g->degree(u); // n<k^2>

        g->forNeighborsOf(u, add_to_pq);

        // trigger all updates of upper bounds
        g->forNeighborsOf(u, [&](node v) {
            if (removed[v] || !candidates.contains(v))
                return;
            auto &data = candidates.at(v);
            data.support += data.upper >= u_data.upper;
            if ((data.nbrs++ == g->degree(v) - 1) && (data.support < data.upper) && !inQueue[v]) {
                triggerUpper.push_back(v);
                inQueue[v] = true;
            }
        });

        while (!triggerUpper.empty()) {
            node v = triggerUpper.front();
            triggerUpper.pop_front();
            inQueue[v] = false;

            auto kold = candidates.at(v).upper;
            auto knew = candidates.at(v).upper = hindex(*g, v, candidates, buf);

            // prune out this node; never in the community
            if (knew < lower_bound) {
                removed[v] = true;
                candidates.erase(v);
                knew = 0;
                potential += g->degree(v) * g->degree(v); // n<k^2>

                // update priority queue
                g->forNeighborsOf(v, [&](node w) {
                    if (pq.contains(w) && pq.getKey(w))
                        pq.changeKey(pq.getKey(w) - 1, w);
                });

                // trigger upper on ony neighors
                g->forNeighborsOf(v, [&](node w) {
                    if (removed[w] || !candidates.contains(w))
                        return;
                    auto &data = candidates.at(w);
                    if (knew < data.upper && data.upper <= kold && --data.support < data.upper
                        && !inQueue[w]) {
                        triggerUpper.push_back(w);
                        inQueue[w] = true;
                    }
                });
            }

            // trigger lower bounds
            if (2ULL * potential >= 3ULL * potential_old) {
                potential_old = potential;

                std::vector<node> nodes;
                nodes.reserve(candidates.size());
                for (auto [k, _] : candidates)
                    nodes.push_back(k);

                auto subg = makeSubgraph(*g, nodes);
                std::set<node> query;
                for (node v : s)
                    query.insert(subg.toLocalId(v));
                lower_bound = SteinerKCore(subg).queryCoreness(query);
            }
        }
    }

    std::vector<node> nodes;
    nodes.reserve(candidates.size());
    for (auto [k, _] : candidates)
        nodes.push_back(k);
    auto subg = makeSubgraph(*g, nodes);
    std::set<node> query;
    for (node v : s)
        query.insert(subg.toLocalId(v));
    std::set<node> comm = SteinerKCore(subg).expandOneCommunity(query);
    std::set<node> comm2;
    for (node v : comm)
        comm2.insert(subg.toOriginalId(v));
    return comm2;
}

} // namespace NetworKit
