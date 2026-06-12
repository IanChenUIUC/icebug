/*
 * SteinerKCore.cpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <stdexcept>
#include <networkit/auxiliary/BucketPQ.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>
#include <networkit/scd/SteinerKCore.hpp>
#include <networkit/structures/UnionFindSubset.hpp>

namespace NetworKit {

SteinerKCore::SteinerKCore(const Graph &g, const std::vector<count> &coreness)
    : SelectiveCommunityDetector(g), coreness(coreness) {}

std::set<node> SteinerKCore::expandOneCommunity(const std::set<node> &s) {
    return run({s})[0];
}

template <class Key, class Value, class F>
decltype(auto) insert(std::map<Key, Value> &m, Key &&key, F &&factory) {
    auto it = m.find(key);
    if (it == m.end())
        it = m.emplace(std::forward<Key>(key), std::forward<F>(factory)()).first;
    return (it->second);
}

using Aux::BucketPQ;
using CountMap = std::vector<std::pair<index, count>>;

// increments the counter for each component, and returns whether it hits size
auto increment = [](CountMap &cm, index qId, count delta, count size) -> bool {
    for (auto it = cm.begin(); it != cm.end(); ++it) {
        if (it->first == qId) {
            it->second += delta;
            if (it->second == size) {
                *it = cm.back();
                cm.pop_back();
                return true;
            }
            return false;
        }
    }

    if (delta == size)
        return true;
    cm.emplace_back(qId, delta);
    return false;
};

auto changeKey = [](auto &m, node from, node to) {
    auto nh = m.extract(from);
    if (!nh.empty()) {
        nh.key() = to;
        m.insert(std::move(nh));
    }
};

std::vector<std::set<node>> SteinerKCore::run(const std::vector<std::set<node>> &s) {
    UnionFindSubset uf(g->numberOfNodes());
    BucketPQ pq(g->numberOfNodes(), 0, static_cast<int64_t>(g->numberOfNodes() - 1));
    std::map<node, CountMap> counts; // counts[componentRepr][queryIdx]

    std::vector<std::vector<index>> singletons(g->numberOfNodes());
    for (index i = 0; i < s.size(); ++i) {
        for (node u : s[i]) {
            if (!pq.contains(u))
                pq.insert(static_cast<int64_t>(coreness[u]), u);
            if (s[i].size() > 1)
                counts[u].emplace_back(i, 1);
            else
                singletons[coreness[u]].push_back(i);
        }
    }

    std::vector<std::set<node>> output(s.size());
    std::vector<bool> visited(g->numberOfNodes(), 0);
    count completed = 0;
    while (completed < s.size()) {
        if (pq.empty())
            throw std::invalid_argument("SteinerKCore::run got disconnected input queries");

        count k = pq.getMax().first;
        std::vector<index> &ready = singletons[k];
        while (!pq.empty() && pq.getMax().first == k) {
            node u = pq.extractMax().second;
            visited[u] = true;

            g->forNeighborsOf(u, [&](node v) {
                if (coreness[v] >= k && uf.find(u) != uf.find(v)) {
                    node to = uf.find(u), from = uf.find(v);
                    if (counts[to].size() < counts[from].size())
                        std::swap(to, from);
                    for (auto &[qId, c] : counts[from]) {
                        if (increment(counts[to], qId, c, s[qId].size()))
                            ready.push_back(qId);
                    }
                    counts.erase(from);
                    uf.merge(u, v);
                    if (uf.find(u) != to)
                        changeKey(counts, to, uf.find(u));
                }
                if (!visited[v] && !pq.contains(v))
                    pq.insert(static_cast<int64_t>(std::min(coreness[v], k)), v);
            });
        }

        for (index qId : ready) {
            std::vector<node> comm = uf.subset(*s[qId].cbegin());
            output[qId] = std::set(comm.begin(), comm.end());
        }
        completed += ready.size();
    }

    return output;
}

} // namespace NetworKit
