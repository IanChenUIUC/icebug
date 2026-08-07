/*
 * LocalKCore.cpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <algorithm>
#include <stdexcept>
#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/scd/LocalKCore.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>
#include <networkit/scd/SteinerKCore.hpp>

#include <tlx/container/ring_buffer.hpp>

#include <networkit/Globals.hpp>
#include <networkit/auxiliary/BucketPQ.hpp>
#include <networkit/auxiliary/SignalHandling.hpp>

namespace NetworKit {

LocalKCore::LocalKCore(const Graph &g, bool maximal, bool tightenUpper)
    : SelectiveCommunityDetector(g), maximal(maximal), tightenUpper(tightenUpper) {
    g->visit([](const auto &g) {
        using T = std::decay_t<decltype(g)>;
        if constexpr (std::is_same_v<T, InducedSubgraphView>)
            throw std::invalid_argument("LocalKCore does not support graph views");
    });
    if (g.isDirected() || g.isWeighted())
        throw std::invalid_argument("LocalKCore requires an undirected, unweighted graph");
    if (g.numberOfSelfLoops())
        throw std::invalid_argument("LocalKCore does not support self loops");
}

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
}

enum class State : uint8_t { unseen, candidate, removed };

struct data_t {
    count upper;   // upper bound on coreness; zero once removed, unread while unseen
    count support; // number of nbrs with ub >= upper; maintained only while a candidate
    State state;
    bool inQueue; // whether it awaits tightening
    bool pushed;  // whether its neighborhood has reached the priority queue
};

// the upper bound on the coreness of any node of the graph
inline count ub(const Graph &g, const std::vector<data_t> &data, node v) {
    const data_t &d = data[v];
    return d.state == State::unseen ? g.degree(v) : d.upper;
}

// the largest h with |{v in N(u) : ub(v) >= h}| >= h, paired with that cardinality
std::pair<count, count> hindex(const Graph &g, node u, const std::vector<data_t> &data,
                               std::vector<count> &buf) {
    buf.assign(g.degree(u) + 1, 0);

    g.forNeighborsOf(u, [&](node v) { ++buf[std::min(ub(g, data, v), g.degree(u))]; });

    count sum = 0, h = g.degree(u);
    for (; h; --h) {
        sum += buf[h];
        if (sum >= h)
            break;
    }
    return {h, h ? sum : g.degree(u)};
}

// minimum degree of the subgraph induced on comm
count minDegree(const Graph &g, const std::set<node> &comm) {
    count k = comm.size();
    for (node u : comm) {
        count deg = 0;
        g.forNeighborsOf(u, [&](node v) { deg += comm.contains(v); });
        k = std::min(k, deg);
    }
    return k;
}

InducedSubgraphView makeSubgraph(const Graph &g, Aux::sorted_unique_t,
                                 std::span<const node> nodes) {
    return g->visit([&](const auto &g) -> InducedSubgraphView {
        using T = std::decay_t<decltype(g)>;

        if constexpr (std::is_same_v<T, InducedSubgraphView>)
            throw std::logic_error("LocalKCore cannot induce a subgraph of a view");
        else {
            auto subg = InducedSubgraphView(g, true);
            subg.addNodes(Aux::sorted_unique, nodes);
            return subg;
        }
    });
}

std::set<node> LocalKCore::expandOneCommunity(const std::set<node> &s) {
    stats.clear();

    std::vector<data_t> data(g->upperNodeIdBound());
    std::vector<node> explored; // every node that has ever been a candidate, in that order

    // queue of nodes that need to be tightened
    tlx::RingBuffer<node> triggerUpper(g->numberOfNodes());

    // store the priority queue based on largest incidence
    Aux::BucketPQ pq(g->upperNodeIdBound(), 0, static_cast<int64_t>(g->numberOfNodes()) - 1);

    std::vector<count> buf;
    count lower_bound = 0; // lower bound on coreness of s
    count outVolume = 0, numRemoved = 0;
    count potential = 0, potential_old = 1;

    auto check = [](bool ok, const char *what) {
        if (!ok)
            throw std::logic_error(std::string("LocalKCore invariant violated: ") + what);
    };

    // drop support of nbrs of v, after tightening data[v].upper
    auto demote = [&](node v, count kold, count knew) {
        assert(knew < kold);
        if (!tightenUpper)
            return;

        g->forNeighborsOf(v, [&](node w) {
            data_t &d = data[w];
            if (d.state != State::candidate)
                return;
            if (knew < d.upper && d.upper <= kold && --d.support < d.upper && !d.inQueue) {
                triggerUpper.push_back(w);
                d.inQueue = true;
            }
        });
        potential += g->degree(v);
    };

    // prune out this node
    auto prune = [&](node v) {
        data_t &d = data[v];
        assert(d.state != State::removed);

        count old = g->degree(v);
        if (d.state == State::candidate) {
            old = d.upper;
            outVolume -= g->degree(v);

            if (d.pushed) {
                g->forNeighborsOf(v, [&](node w) {
                    if (pq.contains(w) && pq.getKey(w))
                        pq.changeKey(pq.getKey(w) - 1, w);
                });
                potential += g->degree(v);
            }
        }

        d.state = State::removed;
        d.upper = 0;
        ++numRemoved;
        if (old)
            demote(v, old, 0);
    };

    // admit this node as a candidate
    auto explore = [&](node u) {
        assert(data[u].state == State::unseen);

        count h = g->degree(u), support = g->degree(u);
        if (tightenUpper) {
            std::tie(h, support) = hindex(*g, u, data, buf);
            assert(support >= h);
            potential += g->degree(u);
        }

        data[u] = data_t{.upper = h,
                         .support = support,
                         .state = State::candidate,
                         .inQueue = false,
                         .pushed = false};
        explored.push_back(u);
        outVolume += g->degree(u);
        if (h < g->degree(u))
            demote(u, g->degree(u), h);
    };

    // add nbrs of u to the priority queue
    auto pushFrontier = [&](node u) {
        assert(!data[u].pushed);
        data[u].pushed = true;

        g->forNeighborsOf(u, [&](node w) {
            if (data[w].state != State::unseen)
                return;
            if (g->degree(w) < lower_bound) {
                prune(w);
                return;
            }
            if (pq.contains(w))
                pq.changeKey(pq.getKey(w) + 1, w);
            else
                pq.insert(1, w);
        });
        potential += g->degree(u);
    };

    // trigger lower bounds
    auto triggerLower = [&]() {
        std::vector<node> nodes;
        nodes.reserve(explored.size());
        for (node v : explored)
            if (data[v].state == State::candidate)
                nodes.push_back(v);
        std::sort(nodes.begin(), nodes.end());

        auto subg = makeSubgraph(*g, Aux::sorted_unique, nodes);
        std::set<node> query;
        for (node v : s)
            query.insert(subg.toLocalId(v));

        std::set<node> local = SteinerKCore(subg).expandOneCommunity(query);
        count k = minDegree(subg, local);
        check(k >= lower_bound, "the lower bound decreased");

        count upper = none;
        for (node v : s)
            upper = std::min(upper, data[v].upper);

        stats.push_back(TriggerStats{.inducedVolume = 2 * subg.numberOfEdges(),
                                     .outVolume = outVolume,
                                     .potential = potential,
                                     .numExplored = explored.size(),
                                     .numCandidates = subg.numberOfNodes(),
                                     .numRemoved = numRemoved,
                                     .lower = k,
                                     .upper = upper});

        std::set<node> comm;
        for (node v : local)
            comm.insert(subg.toOriginalId(v));
        return comm;
    };

    // the community and its coreness, computed on the whole graph
    std::set<node> answer;
    count answerCoreness = 0;
    if (validate) {
        answer = SteinerKCore(*g).expandOneCommunity(s);
        answerCoreness = minDegree(*g, answer);
    }

    auto checkInvariants = [&]() {
        check(lower_bound <= answerCoreness, "the lower bound exceeds the coreness of the query");
        for (node v : answer)
            check(data[v].state != State::removed, "a node of the community was pruned");
        for (node q : s) {
            check(data[q].state == State::candidate, "the query left the candidate set");
            check(data[q].upper >= answerCoreness,
                  "the upper bound of the query is below its coreness");
        }

        for (node u : explored) {
            const data_t &d = data[u];
            if (d.state != State::candidate || !tightenUpper)
                continue;

            check(d.upper >= hindex(*g, u, data, buf).first,
                  "the upper bound is below the h-index of the neighborhood");
            if (answer.contains(u))
                check(d.upper >= answerCoreness,
                      "the upper bound of a community node is below its coreness");

            count support = 0;
            g->forNeighborsOf(u, [&](node v) { support += ub(*g, data, v) >= d.upper; });
            check(d.support == support, "the support miscounts the neighbors above upper");
            check(d.inQueue || d.support >= d.upper,
                  "a node with too little support is not queued for tightening");
        }

        for (node v = 0; v < g->upperNodeIdBound(); ++v) {
            if (!pq.contains(v) || data[v].state == State::removed)
                continue;
            check(data[v].state != State::candidate, "a candidate is still in the priority queue");

            count incidence = 0;
            g->forNeighborsOf(v, [&](node w) { incidence += data[w].state == State::candidate; });
            check(pq.getKey(v) == static_cast<int64_t>(incidence),
                  "a key is not the incidence to the candidates");
        }
    };

    std::vector<node> connector = getConnection(*g, s);
    for (node u : connector)
        explore(u);
    for (node u : connector)
        pushFrontier(u);

    bool done_bounds = false; // whether we know the coreness of the query
    while (!pq.empty() && !(done_bounds && !maximal)) {
        node u = pq.extractMax().second;
        if (data[u].state == State::removed)
            continue;
        if (g->degree(u) < lower_bound) {
            prune(u);
            continue;
        }

        explore(u);

        // do tightening of upper bounds
        while (!triggerUpper.empty()) {
            node v = triggerUpper.front();
            triggerUpper.pop_front();
            if (data[v].state == State::removed)
                continue;

            data_t &d = data[v];
            assert(d.state == State::candidate);
            d.inQueue = false;
            count kold = d.upper;

            auto [h, support] = hindex(*g, v, data, buf);
            potential += g->degree(v);
            assert(support >= h && h <= kold);

            if (h < lower_bound) {
                prune(v);
                continue;
            }

            d.upper = h;
            d.support = support;
            if (h < kold)
                demote(v, kold, h);
        }

        if (data[u].state == State::candidate)
            pushFrontier(u);

        if (!done_bounds && potential >= 2 * potential_old) {
            potential_old = potential;
            std::set<node> comm = triggerLower();
            lower_bound = stats.back().lower;
            done_bounds = stats.back().upper == lower_bound;
            if (done_bounds && !maximal)
                return comm;
        }

        if (validate)
            checkInvariants();
    }

    std::set<node> comm = triggerLower();
    if (validate && maximal)
        check(comm == answer, "the maximal community is not the whole component");
    return comm;
}

} // namespace NetworKit
