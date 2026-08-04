/*
 * InducedSubgraphView.cpp
 *
 *  Created on: June 26th, 2026
 *  Ian Chen (ianchen3@illinois.edu)
 */

#include <cassert>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>

#include <networkit/graph/Graph.hpp>
#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/graph/InducedSubgraphViewImpl.hpp>

namespace NetworKit {

InducedSubgraphView::InducedSubgraphView(std::variant<const GraphW *, const GraphR *> base,
                                         bool compactIds)
    : baseGraph(base), compact(compactIds), n(0), m(0), storedNumberOfSelfLoops(0) {
    onBase([this](const auto &g) {
        inSubset.assign(g.upperNodeIdBound(), false);
        weighted = g.isWeighted();
        directed = g.isDirected();
    });
}

InducedSubgraphView::InducedSubgraphView(const GraphW &base, bool compactIds)
    : InducedSubgraphView(std::variant<const GraphW *, const GraphR *>(&base), compactIds) {}

InducedSubgraphView::InducedSubgraphView(const GraphR &base, bool compactIds)
    : InducedSubgraphView(std::variant<const GraphW *, const GraphR *>(&base), compactIds) {}

count InducedSubgraphView::degree(node v) const {
    assert(hasNode(v));
    if (directed)
        return compact ? inducedInOutDegree.values().at(v).second : inducedInOutDegree.at(v).second;
    return compact ? inducedDegree.values().at(v) : inducedDegree.at(v);
}

count InducedSubgraphView::degreeIn(node v) const {
    assert(hasNode(v));
    if (directed)
        return compact ? inducedInOutDegree.values().at(v).first : inducedInOutDegree.at(v).first;
    return compact ? inducedDegree.values().at(v) : inducedDegree.at(v);
}

bool InducedSubgraphView::isIsolated(node v) const {
    assert(hasNode(v));
    return degree(v) == 0 && (!directed || degreeIn(v) == 0);
}

namespace {

[[noreturn]] void unsupported(const char *what) {
    throw std::runtime_error(std::string(what)
                             + " is not supported on a graph view; realize() into a GraphW");
}

} // namespace

edgeid InducedSubgraphView::edgeId(node, node) const {
    unsupported("edgeId");
}
std::pair<node, node> InducedSubgraphView::edgeById(index) const {
    unsupported("edgeById");
}
index InducedSubgraphView::indexOfNeighbor(node, node) const {
    unsupported("indexOfNeighbor");
}
node InducedSubgraphView::getIthNeighbor(node, index) const {
    unsupported("getIthNeighbor");
}
node InducedSubgraphView::getIthNeighbor(Unsafe, node, index) const {
    unsupported("getIthNeighbor");
}
node InducedSubgraphView::getIthInNeighbor(node, index) const {
    unsupported("getIthInNeighbor");
}
edgeweight InducedSubgraphView::getIthNeighborWeight(node, index) const {
    unsupported("getIthNeighborWeight");
}
edgeweight InducedSubgraphView::getIthNeighborWeight(Unsafe, node, index) const {
    unsupported("getIthNeighborWeight");
}
std::pair<node, edgeweight> InducedSubgraphView::getIthNeighborWithWeight(node, index) const {
    unsupported("getIthNeighborWithWeight");
}
std::pair<node, edgeweight> InducedSubgraphView::getIthNeighborWithWeight(Unsafe, node,
                                                                         index) const {
    unsupported("getIthNeighborWithWeight");
}
std::pair<node, edgeid> InducedSubgraphView::getIthNeighborWithId(node, index) const {
    unsupported("getIthNeighborWithId");
}

bool InducedSubgraphView::checkConsistency() const {
    const auto &subset = getNodeSubset();
    if (subset.size() != n)
        return false;
    if (!std::ranges::is_sorted(subset))
        return false;

    count members = 0;
    for (node v = 0; v < inSubset.size(); ++v)
        members += inSubset[v];
    if (members != n)
        return false;

    count edges = 0, loops = 0;
    for (node v : subset) {
        if (!subsetHasNode(v) || !baseHasNode(v))
            return false;
        count deg = 0;
        forNeighborsOfUnchecked(v, [&](node u) {
            ++deg;
            edges += directed || v <= u;
            loops += v == u;
        });
        if (deg != degree(toLocalId(v)))
            return false;
    }

    return edges == m && loops == storedNumberOfSelfLoops;
}

index InducedSubgraphView::nextMemberNeighbor(node base, index i, bool incoming) const {
    const count deg = incoming ? baseDegreeIn(base) : baseDegree(base);
    for (; i < deg; ++i)
        if (subsetHasNode(incoming ? baseIthInNeighbor(base, i) : baseIthNeighbor(base, i)))
            return i;
    return none;
}

template <bool Weighted>
std::conditional_t<Weighted, std::pair<node, edgeweight>, node>
InducedSubgraphView::memberNeighborAt(node base, index i, bool incoming) const {
    const node v = incoming ? baseIthInNeighbor(base, i) : baseIthNeighbor(base, i);
    if constexpr (!Weighted)
        return toLocalId(v);
    else
        // The base offers no indexed accessor for an in-edge's weight, so it is looked up by
        // endpoint. Only range traversal reaches here; the callback family does not.
        return {toLocalId(v), incoming ? baseWeight(v, base) : baseIthNeighborWeight(base, i)};
}

template std::pair<node, edgeweight> InducedSubgraphView::memberNeighborAt<true>(node, index,
                                                                                bool) const;
template node InducedSubgraphView::memberNeighborAt<false>(node, index, bool) const;

void InducedSubgraphView::addNodes(std::span<const node> nodes) {
    if (std::is_sorted(nodes.begin(), nodes.end())
        && std::ranges::adjacent_find(nodes) == nodes.end()) {
        return addNodes(Aux::sorted_unique, nodes);
    }

    std::vector<node> cleaned(nodes.begin(), nodes.end());
    std::ranges::sort(cleaned);
    cleaned.erase(std::ranges::unique(cleaned).begin(), cleaned.end());
    addNodes(Aux::sorted_unique, cleaned);
}

void InducedSubgraphView::addNodes(Aux::sorted_unique_t, std::span<const node> nodes) {
    auto doAdd = [&](node v) { return !subsetHasNode(v) && baseHasNode(v); };

    // The caller's span is used as-is until the first id that has to be dropped.
    std::vector<node> storage;
    std::span<const node> adds = nodes;
    if (auto it = std::ranges::find_if_not(nodes, doAdd); it != nodes.end()) {
        storage.reserve(nodes.size() - 1);
        storage.assign(nodes.begin(), it);
        std::ranges::copy_if(std::next(it), nodes.end(), std::back_inserter(storage), doAdd);
        adds = storage;
    }

    if (adds.empty())
        return;
    n += adds.size();

    count vol = 0;
    count bdd = 0;

    // Runs before the new ids join the subset, so an edge between two of them is seen once here
    // and once in the pass below.
    for (auto v : adds) {
        if (baseSelfLoops() > storedNumberOfSelfLoops && baseHasEdge(v, v)) {
            ++storedNumberOfSelfLoops;
            vol += !directed;
        }

        if (directed) {
            forNeighborsOfUnchecked(v, [&](node u) { ++inducedInOutDegree[u].first; });
            forInNeighborsOfUnchecked(v, [&](node u) {
                ++inducedInOutDegree[u].second;
                ++vol;
            });
        } else {
            forNeighborsOfUnchecked(v, [&](node u) {
                ++inducedDegree[u];
                ++bdd;
            });
        }
    }

    for (auto v : adds)
        inSubset[v] = true;

    if (directed) {
        auto f = [](node v) { return std::pair<node, std::pair<node, node>>{v, {0, 0}}; };
        auto pairs = adds | std::views::transform(f);
        inducedInOutDegree.insert(Aux::sorted_unique, pairs.begin(), pairs.end());

#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < static_cast<omp_index>(adds.size()); ++i) {
            node deg = 0;
            forInNeighborsOfUnchecked(adds[i], [&](node) { ++deg; });
            inducedInOutDegree[adds[i]].first = deg;
        }

#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < static_cast<omp_index>(adds.size()); ++i) {
            node deg = 0;
            forNeighborsOfUnchecked(adds[i], [&](node) { ++deg; });
            inducedInOutDegree[adds[i]].second = deg;

#pragma omp atomic
            vol += deg;
        }
    } else {
        auto f = [](node v) { return std::pair<node, node>{v, 0}; };
        auto pairs = adds | std::views::transform(f);
        inducedDegree.insert(Aux::sorted_unique, pairs.begin(), pairs.end());

#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < static_cast<omp_index>(adds.size()); ++i) {
            node deg = 0;
            forNeighborsOfUnchecked(adds[i], [&](node) { ++deg; });
            inducedDegree[adds[i]] = deg;

#pragma omp atomic
            vol += deg;
        }
    }

    m += directed ? vol : (vol + bdd) / 2;
}

void InducedSubgraphView::removeNodes(std::span<const node> nodes) {
    if (std::is_sorted(nodes.begin(), nodes.end())
        && std::ranges::adjacent_find(nodes) == nodes.end()) {
        return removeNodes(Aux::sorted_unique, nodes);
    }

    std::vector<node> cleaned(nodes.begin(), nodes.end());
    std::ranges::sort(cleaned);
    cleaned.erase(std::ranges::unique(cleaned).begin(), cleaned.end());
    removeNodes(Aux::sorted_unique, cleaned);
}

void InducedSubgraphView::removeNodes(Aux::sorted_unique_t, std::span<const node> nodes) {
    auto doRemove = [&](node v) { return subsetHasNode(v); };
    std::vector<node> storage;
    std::span<const node> rems = nodes;
    if (auto it = std::ranges::find_if_not(nodes, doRemove); it != nodes.end()) {
        storage.reserve(nodes.size() - 1);
        storage.assign(nodes.begin(), it);
        std::ranges::copy_if(std::next(it), nodes.end(), std::back_inserter(storage), doRemove);
        rems = storage;
    }

    if (rems.empty())
        return;

    count vol = 0;
    for (auto v : rems) {
        if (storedNumberOfSelfLoops > 0 && baseHasEdge(v, v)) {
            --storedNumberOfSelfLoops;
            vol += !directed;
        }
        vol += degree(toLocalId(v));
    }

    // Cleared before the degrees are updated, so an edge inside the batch is not counted twice.
    for (auto v : rems)
        inSubset[v] = false;

    count bdd = 0;
    for (auto v : rems) {
        if (directed) {
            forNeighborsOfUnchecked(v, [&](node u) { --inducedInOutDegree[u].first; });
            forInNeighborsOfUnchecked(v, [&](node u) {
                --inducedInOutDegree[u].second;
                ++vol;
            });
        } else {
            forNeighborsOfUnchecked(v, [&](node u) {
                --inducedDegree[u];
                ++bdd;
            });
        }
    }

    n -= rems.size();
    m -= directed ? vol : (vol + bdd) / 2;

    if (directed)
        Aux::erase_if(inducedInOutDegree, [&](const auto &kv) { return !subsetHasNode(kv.first); });
    else
        Aux::erase_if(inducedDegree, [&](const auto &kv) { return !subsetHasNode(kv.first); });
}

GraphW InducedSubgraphView::realize(bool compactIds) const {
    const node idBound = compactIds ? n : baseUpperNodeIdBound();
    GraphW S(idBound, weighted, directed);

    if (!compactIds)
        for (node v = 0; v < idBound; ++v)
            if (!subsetHasNode(v))
                S.removeNode(v);

    if (compact == compactIds)
        forEdges([&](node u, node v, edgeweight w) { S.addEdge(u, v, w); });
    else if (compactIds)
        forEdges([&](node u, node v, edgeweight w) {
            S.addEdge(rankInSubset(u), rankInSubset(v), w);
        });
    else
        forEdges(
            [&](node u, node v, edgeweight w) { S.addEdge(toOriginalId(u), toOriginalId(v), w); });

    return S;
}

} // namespace NetworKit
