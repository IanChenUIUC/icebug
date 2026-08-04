/*
 * InducedSubgraphViewImpl.hpp
 *
 *  Inline bodies for InducedSubgraphView. Separate from InducedSubgraphView.hpp because these
 *  reach through the handle, which needs the concrete graphs complete.
 */

#ifndef NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_IMPL_HPP_
#define NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_IMPL_HPP_

#include <algorithm>
#include <bit>
#include <numeric>
#include <utility>
#include <vector>

#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/graph/ReferenceGraphImpl.hpp>

namespace NetworKit {

/* THE BASE GRAPH */

template <typename Fn>
decltype(auto) InducedSubgraphView::onBase(Fn &&f) const {
    return std::visit([&f](const auto *g) -> decltype(auto) { return f(*g); }, baseGraph);
}

inline ReferenceGraph InducedSubgraphView::getBaseGraph() const noexcept {
    return onBase([](const auto &g) { return ReferenceGraph(g); });
}

inline bool InducedSubgraphView::baseHasNode(node v) const {
    return onBase([v](const auto &g) { return g.hasNode(v); });
}
inline count InducedSubgraphView::baseDegree(node v) const {
    return onBase([v](const auto &g) { return g.degree(v); });
}
inline count InducedSubgraphView::baseDegreeIn(node v) const {
    return onBase([v](const auto &g) { return g.degreeIn(v); });
}
inline bool InducedSubgraphView::baseHasEdge(node u, node v) const {
    return onBase([u, v](const auto &g) { return g.hasEdge(u, v); });
}
inline edgeweight InducedSubgraphView::baseWeight(node u, node v) const {
    return onBase([u, v](const auto &g) { return g.weight(u, v); });
}
inline count InducedSubgraphView::baseSelfLoops() const {
    return onBase([](const auto &g) { return g.numberOfSelfLoops(); });
}
inline node InducedSubgraphView::baseIthNeighbor(node u, index i) const {
    return onBase([u, i](const auto &g) { return g.getIthNeighbor(unsafe, u, i); });
}
inline node InducedSubgraphView::baseIthInNeighbor(node u, index i) const {
    return onBase([u, i](const auto &g) { return g.getIthInNeighbor(u, i); });
}
inline edgeweight InducedSubgraphView::baseIthNeighborWeight(node u, index i) const {
    return onBase([u, i](const auto &g) { return g.getIthNeighborWeight(unsafe, u, i); });
}

inline bool InducedSubgraphView::getKeepEdgesSorted() const noexcept {
    return onBase([](const auto &g) { return g.getKeepEdgesSorted(); });
}

inline bool InducedSubgraphView::hasEdge(node u, node v) const {
    return hasNode(u) && hasNode(v) && baseHasEdge(toOriginalId(u), toOriginalId(v));
}

inline edgeweight InducedSubgraphView::weight(node u, node v) const {
    return hasEdge(u, v) ? baseWeight(toOriginalId(u), toOriginalId(v)) : nullWeight;
}

inline node InducedSubgraphView::rankInSubset(node u) const noexcept {
    const auto &nodes = getNodeSubset();
    return static_cast<node>(std::lower_bound(nodes.begin(), nodes.end(), u) - nodes.begin());
}

/* NODE ITERATORS */

template <typename L>
void InducedSubgraphView::forNodes(L handle) const {
    const auto &nodes = getNodeSubset();
    if (compact)
        for (index i = 0; i < n; ++i)
            handle(static_cast<node>(i));
    else
        for (index i = 0; i < n; ++i)
            handle(nodes[i]);
}

template <typename C, typename L>
void InducedSubgraphView::forNodesWhile(C condition, L handle) const {
    const auto &nodes = getNodeSubset();
    if (compact)
        for (index i = 0; i < n && condition(); ++i)
            handle(static_cast<node>(i));
    else
        for (index i = 0; i < n && condition(); ++i)
            handle(nodes[i]);
}

template <typename L>
void InducedSubgraphView::forNodesInRandomOrder(L handle) const {
    std::vector<index> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), Aux::Random::getURNG());

    const auto &nodes = getNodeSubset();
    if (compact)
        for (index i : order)
            handle(static_cast<node>(i));
    else
        for (index i : order)
            handle(nodes[i]);
}

template <typename L>
void InducedSubgraphView::forNodePairs(L handle) const {
    const auto &nodes = getNodeSubset();
    if (compact) {
        for (index i = 0; i < n; ++i)
            for (index j = i + 1; j < n; ++j)
                handle(static_cast<node>(i), static_cast<node>(j));
    } else {
        for (index i = 0; i < n; ++i)
            for (index j = i + 1; j < n; ++j)
                handle(nodes[i], nodes[j]);
    }
}

template <typename L>
void InducedSubgraphView::parallelForNodes(L handle) const {
    const auto &nodes = getNodeSubset();
    const auto nodeCount = static_cast<omp_index>(n);
    if (compact) {
#pragma omp parallel for
        for (omp_index i = 0; i < nodeCount; ++i)
            handle(static_cast<node>(i));
    } else {
#pragma omp parallel for
        for (omp_index i = 0; i < nodeCount; ++i)
            handle(nodes[i]);
    }
}

template <typename L>
void InducedSubgraphView::balancedParallelForNodes(L handle) const {
    const auto &nodes = getNodeSubset();
    const auto nodeCount = static_cast<omp_index>(n);
    if (compact) {
#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < nodeCount; ++i)
            handle(static_cast<node>(i));
    } else {
#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < nodeCount; ++i)
            handle(nodes[i]);
    }
}

template <typename L>
void InducedSubgraphView::parallelForNodePairs(L handle) const {
    const auto &nodes = getNodeSubset();
    const auto nodeCount = static_cast<omp_index>(n);
    if (compact) {
#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < nodeCount; ++i)
            for (omp_index j = i + 1; j < nodeCount; ++j)
                handle(static_cast<node>(i), static_cast<node>(j));
    } else {
#pragma omp parallel for schedule(guided)
        for (omp_index i = 0; i < nodeCount; ++i)
            for (omp_index j = i + 1; j < nodeCount; ++j)
                handle(nodes[i], nodes[j]);
    }
}

template <typename L>
double InducedSubgraphView::parallelSumForNodes(L handle) const {
    const auto &nodes = getNodeSubset();
    const auto nodeCount = static_cast<omp_index>(n);
    double sum = 0.0;

    if (compact) {
#pragma omp parallel for reduction(+ : sum)
        for (omp_index i = 0; i < nodeCount; ++i)
            sum += handle(static_cast<node>(i));
    } else {
#pragma omp parallel for reduction(+ : sum)
        for (omp_index i = 0; i < nodeCount; ++i)
            sum += handle(nodes[i]);
    }

    return sum;
}

/// Reads the arm's range directly: the handle's callback family is templated on the callback, and
/// calling it from an arm of the handle would not terminate instantiation.
template <bool Weighted, bool Incoming, typename L>
void InducedSubgraphView::forBaseNeighbors(node baseNode, L handle) const {
    assert(baseHasNode(baseNode));
    const count deg = Incoming ? baseDegreeIn(baseNode) : baseDegree(baseNode);

    // Bisecting once per subset member beats scanning when the subset is much the smaller of the
    // two. Both orders are ascending by base id, so callers cannot tell which ran.
    if (getKeepEdgesSorted() && deg > numberOfNodes() * std::bit_width(deg) * 4) {
        for (node v : getNodeSubset()) {
            const node from = Incoming ? v : baseNode;
            const node to = Incoming ? baseNode : v;
            if (!baseHasEdge(from, to))
                continue;
            if constexpr (Weighted)
                handle(v, baseWeight(from, to));
            else
                handle(v, defaultEdgeWeight);
        }
        return;
    }

    onBase([&](const auto &g) {
        auto neighbors = [&] {
            if constexpr (Incoming)
                return g.template inNeighbors<Weighted>(baseNode);
            else
                return g.template outNeighbors<Weighted>(baseNode);
        }();

        for (const auto nb : neighbors) {
            const node v = neighborTarget(nb);
            if (subsetHasNode(v))
                handle(v, neighborWeight(nb));
        }
    });
}

template <typename L>
void InducedSubgraphView::forNeighborsOfUnchecked(node u, L handle) const {
    forBaseNeighbors<false, false>(u, [&](node v, edgeweight) { handle(v); });
}

template <typename L>
void InducedSubgraphView::forInNeighborsOfUnchecked(node u, L handle) const {
    forBaseNeighbors<false, true>(u, [&](node v, edgeweight) { handle(v); });
}

/* EDGE ITERATORS */

template <bool Dedup, typename L>
void InducedSubgraphView::forLocalOutEdges(node u, L &handle) const {
    auto emit = [&](node v, edgeweight w) {
        const node local = toLocalId(v);
        if constexpr (Dedup)
            if (!directed && u > local)
                return;
        GraphIterationDetail::applyEdge<false>(handle, u, std::pair<node, edgeweight>{local, w});
    };

    const node baseNode = toOriginalId(u);
    if (weighted)
        forBaseNeighbors<true, false>(baseNode, emit);
    else
        forBaseNeighbors<false, false>(baseNode, emit);
}

template <typename L>
void InducedSubgraphView::forEdgesOf(node u, L handle) const {
    forLocalOutEdges<false>(u, handle);
}

template <typename L>
void InducedSubgraphView::forInEdgesOf(node u, L handle) const {
    auto emit = [&](node v, edgeweight w) {
        GraphIterationDetail::applyEdge<false>(handle, u,
                                               std::pair<node, edgeweight>{toLocalId(v), w});
    };

    const node baseNode = toOriginalId(u);
    if (weighted)
        forBaseNeighbors<true, true>(baseNode, emit);
    else
        forBaseNeighbors<false, true>(baseNode, emit);
}

template <typename L>
void InducedSubgraphView::forEdges(L handle) const {
    forNodes([&](node u) { forLocalOutEdges<true>(u, handle); });
}

template <typename L>
void InducedSubgraphView::parallelForEdges(L handle) const {
    const auto &nodes = getNodeSubset();
    const auto nodeCount = static_cast<omp_index>(n);
#pragma omp parallel for schedule(guided)
    for (omp_index i = 0; i < nodeCount; ++i)
        forLocalOutEdges<true>(compact ? static_cast<node>(i) : nodes[i], handle);
}

template <typename L>
double InducedSubgraphView::parallelSumForEdges(L handle) const {
    const auto &nodes = getNodeSubset();
    const auto nodeCount = static_cast<omp_index>(n);
    double sum = 0.0;

#pragma omp parallel for reduction(+ : sum) schedule(guided)
    for (omp_index i = 0; i < nodeCount; ++i) {
        auto accumulate = [&sum, &handle](node s, node t, edgeweight ew, edgeid id) {
            sum += edgeLambda(handle, s, t, ew, id);
        };
        forLocalOutEdges<true>(compact ? static_cast<node>(i) : nodes[i], accumulate);
    }

    return sum;
}

template <bool Weighted>
std::ranges::subrange<ViewNeighborIterator<Weighted>>
InducedSubgraphView::outNeighbors(node u) const {
    using Iter = ViewNeighborIterator<Weighted>;
    const node base = toOriginalId(u);
    return {Iter(this, base, false, nextMemberNeighbor(base, 0, false)),
            Iter(this, base, false, none)};
}

template <bool Weighted>
std::ranges::subrange<ViewNeighborIterator<Weighted>>
InducedSubgraphView::inNeighbors(node u) const {
    using Iter = ViewNeighborIterator<Weighted>;
    if (!directed)
        return outNeighbors<Weighted>(u);

    const node base = toOriginalId(u);
    return {Iter(this, base, true, nextMemberNeighbor(base, 0, true)),
            Iter(this, base, true, none)};
}

template <bool Weighted>
typename ViewNeighborIterator<Weighted>::value_type
ViewNeighborIterator<Weighted>::operator*() const {
    return view->template memberNeighborAt<Weighted>(baseNode, i, incoming);
}

template <bool Weighted>
ViewNeighborIterator<Weighted> &ViewNeighborIterator<Weighted>::operator++() {
    i = view->nextMemberNeighbor(baseNode, i + 1, incoming);
    return *this;
}

static_assert(GraphLike<InducedSubgraphView>);
static_assert(!IndexedGraph<InducedSubgraphView>, "a view stores no edges, so it has no edge ids");
static_assert(!MutableGraph<InducedSubgraphView>,
              "the node subset changes in batches; individual edges do not");

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_IMPL_HPP_
