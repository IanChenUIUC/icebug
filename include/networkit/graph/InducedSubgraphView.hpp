/*
 * InducedSubgraphView.hpp
 *
 *  Created on: 06.25.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_
#define NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_

#include <map>
#include <set>
#include "networkit/graph/EdgeIterators.hpp"
#include "networkit/graph/Graph.hpp"
#include "networkit/graph/NeighborIterators.hpp"
#include "networkit/graph/NodeIterators.hpp"

#include <networkit/graph/GraphR.hpp>

// TODO: add an option to make the IDs contiguous

namespace NetworKit {

/**
 * @ingroup graph
 * InducedSubgraphView - Zero-copy induced subgraph.
 *
 * This class provides a memory-efficient graph view into a
 * base graph, providing the same interface.
 * Nodes can be added or removed as a batch.
 *
 * A mutable GraphW can be realized.
 * Neighbors are computed on-the-fly.
 */
class InducedSubgraphView : public Graph {
    std::reference_wrapper<const Graph> originalGraph;
    std::set<node> nodeSubset; // all nodes guaranteed to be in the original graph

    // store the induced degrees of all nodes in originalGraph for fast access
    // for nodes not in nodeSubset, may contain garbage
    std::map<node, count> inducedDegree;
    std::map<node, count> inducedInDegree;

protected:
    friend class EdgeTypeIterator<InducedSubgraphView, Edge>;
    friend class EdgeTypeIterator<InducedSubgraphView, WeightedEdge>;

    // count n;
    // count m;
    // count storedNumberOfSelfLoops;
    // bool weighted;
    // bool directed;

public:
    /**
     * Construct an induced subgraph view from the original graph and the node subset.
     * @param originalGraph The original CSR graph
     * @param subset The defining nodes of the induced subgraph.
     */
    InducedSubgraphView(const Graph &originalGraph, const std::set<node> &subset);

    /**
     * Add nodes to the subset.
     */
    void addNode(node u) { addNodes({u}); }
    void addNodes(const std::set<node> &subset);

    /**
     * Remove nodes from the subset.
     */
    void removeNode(node u) { removeNodes({u}); }
    void removeNodes(const std::set<node> &subset);

    /**
     * Find all nodes in (V - S) that have source inside the subgraph view, and outNeighbor outside.
     */
    std::set<node> frontier();

    /**
     * Construct an explicit mutable subgraph equivalent to this view.
     * @param compact; whether the subgraph should get compact node IDs
     */
    GraphW realize(bool compact = false) const;

    /**
     * Copy constructor
     */
    InducedSubgraphView(const InducedSubgraphView &other)
        : Graph(other, true), originalGraph(other.originalGraph), nodeSubset(other.nodeSubset),
          inducedDegree(other.inducedDegree), inducedInDegree(other.inducedInDegree) {}

    /**
     * Move constructor
     */
    InducedSubgraphView(const InducedSubgraphView &&other) noexcept
        : Graph(other, true), originalGraph(other.originalGraph), nodeSubset(other.nodeSubset),
          inducedDegree(other.inducedDegree), inducedInDegree(other.inducedInDegree) {}

    /**
     * Copy assignment
     */
    InducedSubgraphView &operator=(const InducedSubgraphView &other) {
        Graph::operator=(other);
        return *this;
    }

    /**
     * Move assignment
     */
    InducedSubgraphView &operator=(InducedSubgraphView &&other) noexcept {
        Graph::operator=(std::move(other));
        return *this;
    }

    /** Default destructor */
    ~InducedSubgraphView() override = default;

    /**
     * Access the current nodes that are in the graph.
     */
    const std::set<node> &getNodeSubset() const noexcept { return nodeSubset; };

    /**
     * Access the underlying graph.
     */
    const Graph &getOriginalGraph() const noexcept { return originalGraph; };

    // For support of API: NetworKit::Graph::NodeIterator
    using NodeIterator = NodeIteratorBase<InducedSubgraphView>;
    // For support of API: NetworKit::Graph::NodeRange
    using NodeRange = NodeRangeBase<InducedSubgraphView>;

    // For support of API: NetworKit::Graph:EdgeIterator
    using EdgeIterator = EdgeTypeIterator<InducedSubgraphView, Edge>;
    // For support of API: NetworKit::InducedSubgraphView:EdgeWeightIterator
    using EdgeWeightIterator = EdgeTypeIterator<InducedSubgraphView, WeightedEdge>;
    // For support of API: NetworKit::InducedSubgraphView:EdgeRange
    using EdgeRange = EdgeTypeRange<InducedSubgraphView, Edge>;
    // For support of API: NetworKit::InducedSubgraphView:EdgeWeightRange
    using EdgeWeightRange = EdgeTypeRange<InducedSubgraphView, WeightedEdge>;

    // For support of API: NetworKit::Graph::NeighborIterator;
    using NeighborIterator = NeighborIteratorBase<std::vector<node>>;
    // For support of API: NetworKit::Graph::NeighborIterator;
    using NeighborWeightIterator =
        NeighborWeightIteratorBase<std::vector<node>, std::vector<edgeweight>>;

    /**
     * Get an iterable range over the nodes of the graph.
     *
     * @return Iterator range over the nodes of the graph.
     */
    NodeRange nodeRange() const noexcept { return NodeRange(*this); }

    /**
     * Get an iterable range over the edges of the graph.
     *
     * @return Iterator range over the edges of the graph.
     */
    EdgeRange edgeRange() const noexcept { return EdgeRange(*this); }

    /**
     * Get an iterable range over the edges of the graph and their weights.
     *
     * @return Iterator range over the edges of the graph and their weights.
     */
    EdgeWeightRange edgeWeightRange() const noexcept { return EdgeWeightRange(*this); }

    /**
     * Iterate over all nodes of the graph and call @a handle (lambda
     * closure).
     *
     * @param handle Takes parameter <code>(node)</code>.
     */
    template <typename L>
    void forNodes(L handle) const;

    /**
     * Iterate randomly over all nodes of the graph and call @a handle (lambda
     * closure).
     *
     * @param handle Takes parameter <code>(node)</code>.
     */
    template <typename L>
    void parallelForNodes(L handle) const;

    /** Iterate over all nodes of the graph and call @a handle (lambda
     * closure) as long as @a condition remains true. This allows for breaking
     * from a node loop.
     *
     * @param condition Returning <code>false</code> breaks the loop.
     * @param handle Takes parameter <code>(node)</code>.
     */
    template <typename C, typename L>
    void forNodesWhile(C condition, L handle) const;

    /**
     * Iterate randomly over all nodes of the graph and call @a handle (lambda
     * closure).
     *
     * @param handle Takes parameter <code>(node)</code>.
     */
    template <typename L>
    void forNodesInRandomOrder(L handle) const;

    /**
     * Iterate in parallel over all nodes of the graph and call handler
     * (lambda closure). Using schedule(guided) to remedy load-imbalances due
     * to e.g. unequal degree distribution.
     *
     * @param handle Takes parameter <code>(node)</code>.
     */
    template <typename L>
    void balancedParallelForNodes(L handle) const;

    /**
     * Iterate over all undirected pairs of nodes and call @a handle (lambda
     * closure).
     *
     * @param handle Takes parameters <code>(node, node)</code>.
     */
    template <typename L>
    void forNodePairs(L handle) const;

    /**
     * Iterate over all undirected pairs of nodes in parallel and call @a
     * handle (lambda closure).
     *
     * @param handle Takes parameters <code>(node, node)</code>.
     */
    template <typename L>
    void parallelForNodePairs(L handle) const;

    /**
     * Iterate in parallel over all nodes and sum (reduce +) the values
     * returned by the handler
     */
    template <typename L>
    double parallelSumForNodes(L handle) const;

    // Override from Graph base
    count degree(node v) const override;
    count degreeIn(node v) const override;
    bool isIsolated(node v) const override;
    edgeweight weightedDegree(node u, bool countSelfLoopsTwice = false) const override;
    edgeweight weightedDegreeIn(node u, bool countSelfLoopsTwice = false) const override;

    index upperNodeIdBound() const noexcept;

    bool hasNode(node v) const noexcept override;
    bool hasEdge(node u, node v) const noexcept override;
    edgeweight weight(node u, node v) const override;

    // FIXME: NOT PLANNED TO IMPLEMENT
    edgeid edgeId(node u, node v) const override;
    node getIthNeighbor(Unsafe, node u, index i) const override;
    edgeweight getIthNeighborWeight(node u, index i) const override;
    node getIthNeighbor(node u, index i) const override;
    node getIthInNeighbor(node u, index i) const override;
    std::pair<node, edgeweight> getIthNeighborWithWeight(node u, index i) const override;
    std::pair<node, edgeid> getIthNeighborWithId(node u, index i) const override;

protected:
    std::vector<node> getNeighborsVector(node u, bool inEdges = false) const override;
    std::pair<std::vector<node>, std::vector<edgeweight>>
    getNeighborsWithWeightsVector(node u, bool inEdges = false) const override;

    // FIXME: NOT PLANNED TO IMPLEMENT
    index indexInInEdgeArray(node v, node u) const override;
    index indexInOutEdgeArray(node u, node v) const override;

private:
    /**
     * @brief Virtual method for edge iteration
     */
    void
    forEdgesVirtualImpl(bool directed, bool weighted, bool hasEdgeIds,
                        std::function<void(node, node, edgeweight, edgeid)> handle) const override;

    /**
     * @brief Virtual method for forEdgesOf
     */
    void forEdgesOfVirtualImpl(
        node u, bool directed, bool weighted, bool hasEdgeIds,
        std::function<void(node, node, edgeweight, edgeid)> handle) const override;

    /**
     * @brief Virtual method for forInEdgesOf
     */
    void forInEdgesOfVirtualImpl(
        node u, bool directed, bool weighted, bool hasEdgeIds,
        std::function<void(node, node, edgeweight, edgeid)> handle) const override;

    /**
     * @brief Virtual method for parallelSumForEdges
     */
    double parallelSumForEdgesVirtualImpl(
        bool directed, bool weighted, bool hasEdgeIds,
        std::function<double(node, node, edgeweight, edgeid)> handle) const override;

    /**
     * Computes the weighted in/out degree of node @a u.
     *
     * @param u Node.
     * @param inDegree whether to compute the in degree or the out degree.
     * @param countSelfLoopsTwice If set to true, self-loops will be counted twice.
     *
     * @return Weighted in/out degree of node @a u.
     */
    edgeweight computeWeightedDegree(node u, bool inDegree = false,
                                     bool countSelfLoopsTwice = false) const;
};

/**
 * Class to iterate over the nodes of a graph.
 * Specialized for the InducedSubgraphView.
 */
template <>
class NodeIteratorBase<InducedSubgraphView> {
    const InducedSubgraphView *G;
    std::set<node>::iterator iter;

public:
    // The value type of the nodes (i.e. nodes). Returned by
    // operator*().
    using value_type = node;

    // Reference to the value_type, required by STL.
    using reference = value_type &;

    // Pointer to the value_type, required by STL.
    using pointer = value_type *;

    // STL iterator category.
    using iterator_category = std::forward_iterator_tag;

    // Signed integer type of the result of subtracting two pointers,
    // required by STL.
    using difference_type = ptrdiff_t;

    // Own type.
    using self = NodeIteratorBase;

    NodeIteratorBase(const InducedSubgraphView *G, node u)
        : G(G), iter(G->getNodeSubset().lower_bound(u)) {}

    /**
     * @brief WARNING: This constructor is required for Python and should not be used as the
     * iterator is not initialized.
     */
    NodeIteratorBase() : G(nullptr) {}

    ~NodeIteratorBase() = default;

    NodeIteratorBase &operator++() {
        assert(iter != G->getNodeSubset().end());
        ++iter;
        return *this;
    }

    NodeIteratorBase operator++(int) {
        const auto tmp = *this;
        ++(*this);
        return tmp;
    }

    NodeIteratorBase &operator--() {
        assert(iter != G->getNodeSubset().begin());
        --iter;
        return *this;
    }

    NodeIteratorBase operator--(int) {
        const auto tmp = *this;
        --(*this);
        return tmp;
    }

    bool operator==(const NodeIteratorBase &rhs) const noexcept { return iter == rhs.iter; }

    bool operator!=(const NodeIteratorBase &rhs) const noexcept { return !(*this == rhs); }

    node operator*() const noexcept {
        assert(iter != G->getNodeSubset().end());
        return *iter;
    }
};

template <typename EdgeType>
class EdgeTypeIterator<InducedSubgraphView, EdgeType> {
    using NeighborType =
        std::conditional_t<std::is_same_v<EdgeType, Edge>, InducedSubgraphView::NeighborIterator,
                           InducedSubgraphView::NeighborWeightIterator>;
    using NeighborRangeType =
        std::conditional_t<std::is_same_v<EdgeType, Edge>, InducedSubgraphView::OutNeighborRange,
                           InducedSubgraphView::OutNeighborWeightRange>;

protected:
    const InducedSubgraphView *G;
    NodeIteratorBase<InducedSubgraphView> nodeIter;
    NeighborType nbrIter;
    NeighborRangeType nbrRange;
    index i{none};

    NeighborRangeType makeRange(node u) const {
        if constexpr (std::is_same_v<EdgeType, Edge>)
            return Graph::NeighborRange<false>(*G, u);
        else
            return Graph::NeighborWeightRange<false>(*G, u);
    }

public:
    using GraphType = InducedSubgraphView;

    // The value type of the edges (i.e. a pair). Returned by operator*().
    using value_type = EdgeType;

    // Reference to the value_type, required by STL.
    using reference = value_type &;

    // Pointer to the value_type, required by STL.
    using pointer = value_type *;

    // STL iterator category.
    using iterator_category = std::forward_iterator_tag;

    // Signed integer type of the result of subtracting two pointers,
    // required by STL.
    using difference_type = ptrdiff_t;

    // Own type.
    using self = EdgeTypeIterator;

    EdgeTypeIterator(const GraphType *G, NodeIteratorBase<GraphType> nodeIter)
        : G(G), nodeIter(nodeIter), i(index{0}) {
        if (nodeIter != G->nodeRange().end()) {
            nbrRange = makeRange(*nodeIter);
            nbrIter = nbrRange.begin();
            if (!G->degree(*nodeIter))
                nextEdge();
        }
    }

    EdgeTypeIterator() {};

    bool operator==(const EdgeTypeIterator &rhs) const noexcept {
        return nodeIter == rhs.nodeIter && i == rhs.i;
    }

    bool operator!=(const EdgeTypeIterator &rhs) const noexcept { return !(*this == rhs); }

    // The returned edge depends on both the type of edge and graph
    EdgeType operator*() const noexcept {
        assert(nodeIter != G->nodeRange().end());
        if constexpr (std::is_same_v<EdgeType, Edge>)
            return Edge(*nodeIter, *nbrIter);
        else
            return WeightedEdge(*nodeIter, (*nbrIter).first, (*nbrIter).second);
    }

    EdgeTypeIterator &operator++() {
        nextEdge();
        return *this;
    }

    EdgeTypeIterator operator++(int) {
        const auto tmp = *this;
        ++(*this);
        return tmp;
    }

    EdgeTypeIterator operator--() {
        prevEdge();
        return *this;
    }

    EdgeTypeIterator operator--(int) {
        const auto tmp = *this;
        --(*this);
        return tmp;
    }

    bool validEdge() const noexcept {
        if constexpr (std::is_same_v<EdgeType, Edge>)
            return G->isDirected() || *nodeIter <= *nbrIter;
        else
            return G->isDirected() || *nodeIter <= (*nbrIter).first;
    }

private:
    void nextEdge() {
        do {
            if (++i >= G->degree(*nodeIter)) {
                i = 0;
                do {
                    assert(nodeIter != G->nodeRange().end());
                    ++nodeIter;
                    if (nodeIter == G->nodeRange().end())
                        return;
                    nbrRange = makeRange(*nodeIter);
                    nbrIter = nbrRange.begin();
                } while (!G->degree(*nodeIter));
            } else {
                ++nbrIter;
            }
        } while (!validEdge());
    }

    void prevEdge() {
        do {
            if (!i) {
                do {
                    assert(nodeIter != G->nodeRange().begin());
                    --nodeIter;
                    nbrRange = makeRange(*nodeIter);
                    nbrIter = nbrRange.end();
                } while (!G->degree(*nodeIter));

                i = G->degree(*nodeIter);
                nbrRange = makeRange(*nodeIter);
                nbrIter = nbrRange.end();
            }
            --i;
            --nbrIter;
        } while (!validEdge());
    }
};

template <typename L>
void InducedSubgraphView::forNodes(L handle) const {
    for (auto it = nodeSubset.begin(); it != nodeSubset.end(); ++it)
        handle(*it);
}

template <typename C, typename L>
void InducedSubgraphView::forNodesWhile(C condition, L handle) const {
    for (auto it = nodeSubset.begin(); it != nodeSubset.end() && condition(); ++it)
        handle(*it);
}

template <typename L>
void InducedSubgraphView::forNodesInRandomOrder(L handle) const {
    std::vector<node> randVec(nodeSubset.begin(), nodeSubset.end());
    std::ranges::shuffle(randVec, Aux::Random::getURNG());
    for (node v : randVec)
        handle(v);
}

template <typename L>
void InducedSubgraphView::forNodePairs(L handle) const {
    for (auto it1 = nodeSubset.begin(); it1 != nodeSubset.end(); ++it1)
        for (auto it2 = std::next(it1); it2 != nodeSubset.end(); ++it2)
            handle(*it1, *it2);
}

template <typename L>
void InducedSubgraphView::parallelForNodes(L handle) const {
    WARN("parallelForNodes not implemented for InducedSubgraphView... fallback to sequential");
    return forNodes(handle);
}

template <typename L>
void InducedSubgraphView::balancedParallelForNodes(L handle) const {
    WARN("balancedParallelForNodes not implemented for InducedSubgraphView... fallback to "
         "sequential");
    return forNodes(handle);
}

template <typename L>
void InducedSubgraphView::parallelForNodePairs(L handle) const {
    WARN("parallelForNodePairs not implemented for InducedSubgraphView... fallback to "
         "sequential");
    return forNodePairs(handle);
}

template <typename L>
double InducedSubgraphView::parallelSumForNodes(L handle) const {
    WARN("parallelSumForNodes not implemented for InducedSubgraphView... fallback to "
         "sequential");
    double sum = 0.0;
    forNodes([&](node u) { sum += handle(u); });
    return sum;
}

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_
