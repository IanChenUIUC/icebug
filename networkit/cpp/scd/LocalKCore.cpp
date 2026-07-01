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
#include "networkit/auxiliary/Log.hpp"

namespace NetworKit {

LocalKCore::LocalKCore(const Graph &g, const std::string &logFile)
    : SelectiveCommunityDetector(g), logFile(logFile) {}

count hindex(const Graph &g, node u, const std::unordered_map<node, count> &uppers,
             std::vector<count> &buf) {
    if (g.degree(u) == 0)
        return 0;

    buf.resize(0);
    buf.resize(g.degree(u) + 1); // zero-out

    g.forNeighborsOf(u, [&](node v) { ++buf[std::min(uppers.at(v), g.degree(u))]; });
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

template <typename T>
std::string toString(tlx::RingBuffer<T> rb) {
    std::ostringstream oss;
    oss << "[";

    bool first = true;
    while (!rb.empty()) {
        if (!first)
            oss << ", ";
        first = false;

        oss << rb.front();
        rb.pop_front();
    }

    oss << "]";
    return oss.str();
}

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    assert(!s.empty());

    TRACE("expandOneCommunity called on ", s);

    const bool doLog = !logFile.empty();
    count timeHIndex = 0;    // (us) step (b): h-index + count/push maintenance
    count timeLower = 0;     // (us) step (c): SteinerKCore::queryCoreness + prune sweep
    count timeFinal = 0;     // (us) final SteinerKCore::expandOneCommunity
    count timeOther = 0;     // (us) derived: total - (hindex + lower + final)
    count totalExplored = 0; // distinct nodes ever admitted

    // (admissions, lb) sampled at each refresh; only populated when logging
    std::vector<std::pair<count, count>> lbCurve;

    Aux::Timer total;   // whole-function wall clock
    Aux::Timer section; // reused per bucketed section
    if (doLog)
        total.start();
    auto tic = [&]() {
        if (doLog)
            section.start();
    };
    auto toc = [&](count &bucket) {
        if (doLog) {
            section.stop();
            bucket += section.elapsedMicroseconds();
        }
    };

    // 1. BFS until find connected subgraph containing s
    InducedSubgraphView subg = connectedSubgraph(*g, s);

    // 2. initiate all variables
    count lb = 0;
    std::unordered_map<node, count> ub;
    subg.forNodes([&](node u) { ub[u] = g->degree(u); });
    totalExplored = subg.numberOfNodes(); // seed + connector already admitted

    count vol = 2 * subg.numberOfEdges();
    count threshold = 0;

    // TODO: dynamically resizing the queue might be a good idea, to account for local
    tlx::RingBuffer<node> queue(g->numberOfNodes());
    std::vector<bool> inQueue(g->upperNodeIdBound());
    std::vector<bool> pruned(g->upperNodeIdBound());
    std::unordered_map<node, count> counts;

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

    auto compute_count = [&](node u) {
        auto nbr = subg.neighborRange(u);
        counts[u] = std::count_if(nbr.begin(), nbr.end(), [&](node v) { return ub[v] >= ub[u]; });
    };

    auto remove_node = [&](node u) {
        pruned[u] = true;
        subg.forNeighborsOf(u, [&](node v) {
            if (ub[u] >= ub[v] && --counts[v] < ub[v] && !inQueue[v])
                push(v);
        });
        subg.removeNode(u);
    };

    // 3. loop until convergence (or found bound, if maximal is not set to True):
    // | a) add the frontier
    // | b) update upper-bounds on all nodes
    // | c) if threshold passes: update lower bounds and prune
    std::vector<count> buf;
    bool frontierDone = false;
    subg.forNodes([&](node u) { push(u); });
    while (queue.size() || !frontierDone) {
        // store the nodes that will be get h-index propagated in this round
        count iters = queue.size();
        TRACE("[LOCAL KCORE] queue = ", toString(queue));

        // a) add the frontier
        if (!frontierDone) {
            std::set<node> frontier = subg.frontier();
            std::erase_if(frontier, [&](node u) {
                return pruned[u] || (pruned[u] = (ub[u] = g->degree(u)) < lb);
            });

            // add to the queue for the next phase
            for (node u : frontier) {
                subg.addNode(u);
                ++totalExplored;
                vol += 2 * subg.degree(u);
                compute_count(u);
                subg.forNeighborsOf(u, [&](node v) { counts[v] += ub[u] >= ub[v]; });
                push(u);
            }

            frontierDone = frontier.empty();
            TRACE("[LOCAL KCORE] Added ", frontier, " to frontier");
        }

        // b) update the upper-bounds of all nodes  ---- hindex ----
        // TODO: convert to vector and parallelize, if there are enough nodes
        tic();
        for (index i = 0; i < iters; ++i) {
            node u = pop();
            if (pruned[u])
                continue;

            // update u
            count kold = ub[u];
            count knew = ub[u] = hindex(subg, u, ub, buf);
            if (knew < lb) {
                TRACE("[LOCAL KCORE] Removed ", u);
                remove_node(u);
            } else if (knew < kold) {
                TRACE("[LOCAL KCORE] Updated coreness of node ", u, " to ", knew);
                // update nbr(u)
                compute_count(u);
                subg.forNeighborsOf(u, [&](node v) {
                    if (knew < ub[v] && ub[v] <= kold && --counts[v] < ub[v] && !inQueue[v])
                        push(v);
                });
            }
        }
        toc(timeHIndex);

        // c) if threshold passes: update lower bounds and prune  ---- lower ----
        if (vol > threshold) {
            tic();
            threshold = 2 * vol;
            lb = SteinerKCore(subg).queryCoreness(s);

            if (doLog)
                lbCurve.emplace_back(totalExplored, lb);

            std::vector<node> toPrune;
            subg.forNodes([&](node v) {
                if (ub[v] < lb)
                    toPrune.push_back(v);
            });
            for (auto v : toPrune)
                remove_node(v);

            TRACE("[LOCAL KCORE] Triggered lowerbound update to ", lb);
            TRACE("[LOCAL KCORE] Pruned ", toPrune);
            toc(timeLower);
        }
    }

    // ---- final ----
    tic();
    TRACE("[LOCAL KCORE] Final search on ", subg.getNodeSubset());
    std::set<node> result = SteinerKCore(subg).expandOneCommunity(s);
    toc(timeFinal);

    if (doLog) {
        total.stop();
        count timeTotal = total.elapsedMicroseconds();
        timeOther = timeTotal - timeHIndex - timeLower - timeFinal;

        std::ofstream out(logFile, std::ios::app);
        if (!out)
            throw std::runtime_error("LocalKCore: cannot open logFile " + logFile);

        out << "query_size=" << s.size() << '\n'
            << "total_explored=" << totalExplored << '\n'
            << "community_size=" << result.size() << '\n'
            << "waste=" << (totalExplored - result.size()) << '\n'
            << "final_lb=" << lb << '\n';

        out << "lb_curve=";
        for (size_t i = 0; i < lbCurve.size(); ++i) {
            if (i)
                out << ';';
            out << lbCurve[i].first << ':' << lbCurve[i].second;
        }
        out << '\n';

        out << "time_hindex_us=" << timeHIndex << '\n'
            << "time_lower_us=" << timeLower << '\n'
            << "time_final_us=" << timeFinal << '\n'
            << "time_other_us=" << timeOther << '\n'
            << "time_total_us=" << timeTotal << '\n'
            << "---\n";
    }

    return result;
}

} // namespace NetworKit
