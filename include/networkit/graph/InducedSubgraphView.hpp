/*
 * InducedSubgraphView.hpp
 *
 *  A graph over a subset of another graph's nodes, computed on demand.
 */

#ifndef NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_
#define NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_

#include <cassert>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <networkit/Globals.hpp>
#include <networkit/auxiliary/FlatMap.hpp>
#include <networkit/graph/AttributedGraphBase.hpp>
#include <networkit/graph/GraphConcepts.hpp>
#include <networkit/graph/GraphIterationMixin.hpp>
#include <networkit/graph/ReferenceGraph.hpp>

namespace NetworKit {

class GraphR;
class GraphW;

/**
 * @ingroup graph
 * The subgraph induced by a set of nodes, without materializing it.
 *
 * Non-owning: the graph being viewed must outlive the view. Nodes are added and removed in batches;
 * edges are those of the base graph with both endpoints in the subset, recomputed on each traversal
 * rather than stored.
 *
 * Node ids are the base graph's unless the view is @a compact, in which case a node's id is its rank
 * in getNodeSubset() and ids run from 0 to numberOfNodes() - 1. Compact ids matter to algorithms
 * that allocate by upperNodeIdBound() or that pick a strategy from whether the ids are dense.
 */
class InducedSubgraphView final : public GraphIterationMixin<InducedSubgraphView>,
                                  public AttributedGraphBase<InducedSubgraphView> {
    /*
     * Narrower than ReferenceGraph, which also has a view arm. Were the base reached through the
     * full handle, a template member of the handle would instantiate the view arm, whose body
     * reaches the handle again with a fresh callback type, and the instantiation would not
     * terminate. Excluding the arm here is what bounds it.
     */
    std::variant<const GraphW *, const GraphR *> baseGraph;

    /// Keyed by base-graph id; keys() is sorted, so a compact local id indexes keys() and values().
    Aux::flat_map<node, node> inducedDegree;
    Aux::flat_map<node, std::pair<node, node>> inducedInOutDegree;

    /// Membership, indexed by base-graph id.
    std::vector<bool> inSubset;

    bool compact, weighted, directed;
    count n, m, storedNumberOfSelfLoops;

public:
    /**
     * A view of @a base holding no nodes.
     *
     * @param compactIds whether ids run 0..numberOfNodes()-1 rather than being the base graph's.
     */
    explicit InducedSubgraphView(const GraphW &base, bool compactIds = false);
    explicit InducedSubgraphView(const GraphR &base, bool compactIds = false);

    /// Deleted so that a handle, which may hold a view, cannot silently convert to a GraphW copy.
    InducedSubgraphView(const ReferenceGraph &, bool = false) = delete;

    /* SUBSET MAINTENANCE */

    /**
     * Add nodes, given as base-graph ids, to the subset.
     *
     * Ids already present or absent from the base graph are ignored. The sorted_unique_t overload
     * skips the cleaning pass and requires @a nodes to be ascending with no repeats.
     */
    void addNode(node u) { addNodes({{u}}); }
    void addNodes(std::span<const node> nodes);
    void addNodes(Aux::sorted_unique_t, std::span<const node> nodes);

    /// Remove nodes, given as base-graph ids, from the subset. Ids not present are ignored.
    void removeNode(node u) { removeNodes({{u}}); }
    void removeNodes(std::span<const node> nodes);
    void removeNodes(Aux::sorted_unique_t, std::span<const node> nodes);

    /**
     * Materialize an equivalent graph, copying the induced edges out of the base graph.
     *
     * @param compactIds whether the result has ids 0..numberOfNodes()-1, independently of whether
     * this view does.
     */
    GraphW realize(bool compactIds = false) const;

    /* GLOBAL PROPERTIES */

    count numberOfNodes() const noexcept { return n; }
    count numberOfEdges() const noexcept { return m; }
    count numberOfSelfLoops() const noexcept { return storedNumberOfSelfLoops; }
    index upperNodeIdBound() const noexcept { return compact ? n : baseUpperNodeIdBound(); }
    bool isEmpty() const noexcept { return !n; }
    bool isWeighted() const noexcept { return weighted; }
    bool isDirected() const noexcept { return directed; }

    /// Always false: a view stores no edges, so it assigns no edge ids.
    bool hasEdgeIds() const noexcept { return false; }
    index upperEdgeIdBound() const noexcept { return 0; }

    bool getMaintainCompactEdges() const noexcept { return true; }
    bool hasCompactIds() const noexcept { return compact; }

    /// Recomputes the cached counts from the base graph and reports whether they still agree.
    bool checkConsistency() const;

    /// True when the base graph's neighborhoods are sorted; filtering preserves the order.
    bool getKeepEdgesSorted() const noexcept;
    bool hasSortedNeighborhoods() const noexcept { return getKeepEdgesSorted(); }

    /* NODE AND EDGE PROPERTIES */

    bool hasNode(node v) const noexcept { return compact ? v < n : subsetHasNode(v); }
    count degree(node v) const;
    count degreeIn(node v) const;
    count degreeOut(node v) const { return degree(v); }
    bool isIsolated(node v) const;

    /* INDEXED ACCESS */

    /// All throw: a view has no edge ids, and a position in its neighborhood indexes no array.
    edgeid edgeId(node u, node v) const;
    std::pair<node, node> edgeById(index id) const;
    index indexOfNeighbor(node u, node v) const;
    node getIthNeighbor(node u, index i) const;
    node getIthNeighbor(Unsafe, node u, index i) const;
    node getIthInNeighbor(node u, index i) const;
    edgeweight getIthNeighborWeight(node u, index i) const;
    edgeweight getIthNeighborWeight(Unsafe, node u, index i) const;
    std::pair<node, edgeweight> getIthNeighborWithWeight(node u, index i) const;
    std::pair<node, edgeweight> getIthNeighborWithWeight(Unsafe, node u, index i) const;
    std::pair<node, edgeid> getIthNeighborWithId(node u, index i) const;

    /* IDS AND MEMBERSHIP */

    /// The subset, always as base-graph ids, in ascending order.
    const std::vector<node> &getNodeSubset() const noexcept {
        return directed ? inducedInOutDegree.keys() : inducedDegree.keys();
    }

    /// Whether @a u, a base-graph id, is in the subset.
    bool subsetHasNode(node u) const noexcept { return u < inSubset.size() && inSubset[u]; }

    /// The base-graph id of @a u; the identity unless the view is compact.
    node toOriginalId(node u) const noexcept { return compact ? getNodeSubset()[u] : u; }

    /// The inverse of toOriginalId(); O(log n) when compact, the identity otherwise.
    node toLocalId(node u) const noexcept { return compact ? rankInSubset(u) : u; }

    /// The position of base-graph id @a u in the subset, whether or not this view is compact.
    node rankInSubset(node u) const noexcept;

    /// A handle on the graph being viewed. Returned by value; the view stores no handle.
    ReferenceGraph getBaseGraph() const noexcept;

    /* NODE ITERATORS */

    /// These walk the subset; the mixin would walk the whole base id space and filter.

    template <typename L>
    void forNodes(L handle) const;

    template <typename C, typename L>
    void forNodesWhile(C condition, L handle) const;

    template <typename L>
    void forNodesInRandomOrder(L handle) const;

    template <typename L>
    void forNodePairs(L handle) const;

    template <typename L>
    void parallelForNodes(L handle) const;

    template <typename L>
    void balancedParallelForNodes(L handle) const;

    template <typename L>
    void parallelForNodePairs(L handle) const;

    template <typename L>
    double parallelSumForNodes(L handle) const;

    /* EDGE ITERATORS */

    /// These resolve the base's type once per node; the mixin would resolve it per neighbor.

    template <typename L>
    void forEdgesOf(node u, L handle) const;

    template <typename L>
    void forInEdgesOf(node u, L handle) const;

    template <typename L>
    void forEdges(L handle) const;

    template <typename L>
    void parallelForEdges(L handle) const;

    template <typename L>
    double parallelSumForEdges(L handle) const;

    /* LOOKUPS */

    /// Defers to the base graph, which can bisect a sorted neighborhood.
    bool hasEdge(node u, node v) const;

    edgeweight weight(node u, node v) const;

    /* NEIGHBORHOODS */

    /**
     * The out-neighbors of @a u that are in the subset, as local ids.
     *
     * Computed as it is walked; the range borrows this view, which must outlive it.
     *
     * @tparam Weighted select the (node, weight) payload. Requires isWeighted().
     */
    template <bool Weighted>
    std::ranges::subrange<ViewNeighborIterator<Weighted>> outNeighbors(node u) const;

    /// The in-neighbors of @a u. On an undirected graph these are the out-neighbors.
    template <bool Weighted>
    std::ranges::subrange<ViewNeighborIterator<Weighted>> inNeighbors(node u) const;

    /* CURSOR SUPPORT */

    /**
     * The position of the first subset member at or after @a i in @a base's neighborhood, or none.
     *
     * Positions index the base graph's neighborhood, in its order.
     */
    index nextMemberNeighbor(node base, index i, bool incoming) const;

    /// The payload for the neighbor at position @a i, which must be a subset member.
    template <bool Weighted>
    std::conditional_t<Weighted, std::pair<node, edgeweight>, node>
    memberNeighborAt(node base, index i, bool incoming) const;

private:
    InducedSubgraphView(std::variant<const GraphW *, const GraphR *> base, bool compactIds);

    index baseUpperNodeIdBound() const noexcept { return inSubset.size(); }

    /* THE BASE GRAPH */

    /// Calls @a f with the base graph's concrete type, instantiating the body once per arm.
    template <typename Fn>
    decltype(auto) onBase(Fn &&f) const;

    bool baseHasNode(node v) const;
    count baseDegree(node v) const;
    count baseDegreeIn(node v) const;
    bool baseHasEdge(node u, node v) const;
    edgeweight baseWeight(node u, node v) const;
    count baseSelfLoops() const;
    node baseIthNeighbor(node u, index i) const;
    node baseIthInNeighbor(node u, index i) const;
    edgeweight baseIthNeighborWeight(node u, index i) const;

    /**
     * The subset members among @a u's neighbors, taking and yielding base-graph ids.
     *
     * Reads the neighborhood of a node that need not itself be in the subset, which is what the
     * incremental degree bookkeeping in addNodes() and removeNodes() needs.
     */
    template <typename L>
    void forNeighborsOfUnchecked(node u, L handle) const;

    template <typename L>
    void forInNeighborsOfUnchecked(node u, L handle) const;

    /// The shared body of both; always passes a weight, synthesizing it when @a Weighted is false.
    template <bool Weighted, bool Incoming, typename L>
    void forBaseNeighbors(node base, L handle) const;

    /// Out-edges of local node @a u; @a Dedup drops the second sighting of an undirected edge.
    template <bool Dedup, typename L>
    void forLocalOutEdges(node u, L &handle) const;
};

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_
