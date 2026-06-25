/*
 * InducedSubgraphView.hpp
 *
 *  Created on: 06.25.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 *
 *  Zero-copy induced subgraph.
 */

#ifndef NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_
#define NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_

#include <networkit/graph/Graph.hpp>

namespace NetworKit {

/**
 * @ingroup graph
 * InducedSubgraphView - Zero-copy induced subgraph.
 *
 * This class provides a memory-efficient graph view into a
 * base graph, providing the same interface.
 * Nodes can be added and removed efficiently.
 *
 * A mutable GraphW can be realized.
 */
class InducedSubgraphView : public Graph {
public:
    /**
     * Construct an induced subgraph view from the original graph and the node subset.
     * @param originalGraph The original CSR graph
     * @param subset The defining nodes of the induced subgraph.
     */
    InducedSubgraphView(const Graph &originalGraph, std::vector<node> subset);

    /**
     * Construct an explicit mutable subgraph equivalent to this view.
     */
    GraphW Realize() const;

    /**
     * Copy constructor
     */
    InducedSubgraphView(const InducedSubgraphView &other) : Graph(other) {}

    /**
     * Move constructor
     */
    InducedSubgraphView(InducedSubgraphView &&other) noexcept : Graph(std::move(other)) {}

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

    // Implement pure virtual methods from Graph base class

    count degree(node v) const override;
    count degreeIn(node v) const override;
    bool isIsolated(node v) const override;

    /**
     * Return edge weight of edge {@a u,@a v}. Returns 0 if edge does not
     * exist. If weights are provided during construction, returns the actual
     * edge weight; otherwise returns defaultEdgeWeight (1.0).
     *
     * @param u Endpoint of edge.
     * @param v Endpoint of edge.
     * @return Edge weight of edge {@a u,@a v} or 0 if edge does not exist.
     */
    edgeweight weight(node u, node v) const override;

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

    index indexInInEdgeArray(node v, node u) const override;
    index indexInOutEdgeArray(node u, node v) const override;
};

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_INDUCED_SUBGRAPH_VIEW_HPP_
