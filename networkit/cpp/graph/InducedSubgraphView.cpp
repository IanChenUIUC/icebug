/*
 * InducedSubgraphView.cpp
 *
 *  Created on: June 26th, 2026
 *  Ian Chen (ianchen3@illinois.edu)
 */

#include "networkit/graph/GraphTools.hpp"
#include "networkit/graph/GraphW.hpp"
#include <networkit/graph/InducedSubgraphView.hpp>

// TODO: check if attributes work transparently, or if special handling needs to be done
// TODO: document that EdgeIterators probably won't work (getIthNeighbor not impl-ed)
//   but because we only need ++ and --, we can probably still implement this.

namespace NetworKit {

InducedSubgraphView::InducedSubgraphView(const Graph &originalGraph, const std::set<node> &subset)
    : Graph(0, originalGraph.isWeighted(), originalGraph.isDirected()),
      originalGraph(originalGraph) {
    AddNodes(subset);
}

std::set<node> InducedSubgraphView::boundary() {
    std::set<node> result;
    for (node u : nodeSubset) {
        originalGraph.forNeighborsOf(u, [&](node v) {
            if (!hasNode(v))
                result.insert(v);
        });
    }
    return result;
}

count InducedSubgraphView::degree(node v) const {
    assert(hasNode(v));
    return inducedDegree[v];
}

count InducedSubgraphView::degreeIn(node v) const {
    assert(hasNode(v));
    return isDirected() ? inducedInDegree[v] : degree(v);
};

bool InducedSubgraphView::isIsolated(node v) const {
    assert(hasNode(v));
    return degree(v) == 0 && (!directed || degreeIn(v) == 0);
}

bool InducedSubgraphView::hasNode(node v) const noexcept {
    return nodeSubset.contains(v);
}

bool InducedSubgraphView::hasEdge(node u, node v) const noexcept {
    return hasNode(u) && hasNode(v) && originalGraph.hasEdge(v, v);
}

edgeweight InducedSubgraphView::weight(node u, node v) const {
    if (!hasEdge(u, v))
        return 0;
    return originalGraph.weight(u, v);
}

void InducedSubgraphView::AddNodes(const std::set<node> &subset) {
    std::vector<bool> added(subset.size(), true);

    for (auto it = subset.begin(); it != subset.end(); ++it) {
        if (hasNode(*it)) {
            added[std::distance(subset.begin(), it)] = false;
            continue;
        }
        nodeSubset.insert(*it);
    }

    for (auto it = subset.begin(); it != subset.end(); ++it) {
        if (!added[std::distance(subset.begin(), it)])
            continue;
        count degree = 0;
        originalGraph.forNeighborsOf(*it, [&](node u) {
            if (!hasNode(u))
                return;
            if (u == *it)
                ++storedNumberOfSelfLoops;
            ++inducedDegree[u];
            ++degree;
        });
        inducedDegree[*it] = degree;
        m += degree;
        n += 1;
    }
}

void InducedSubgraphView::RemoveNodes(const std::set<node> &subset) {
    std::vector<bool> removed(subset.size(), true);

    for (auto it = subset.begin(); it != subset.end(); ++it) {
        if (!hasNode(*it)) {
            removed[std::distance(subset.begin(), it)] = false;
            continue;
        }
        nodeSubset.erase(*it);
    }

    for (auto it = subset.begin(); it != subset.end(); ++it) {
        if (!removed[std::distance(subset.begin(), it)])
            continue;
        count degree = 0;
        originalGraph.forNeighborsOf(*it, [&](node u) {
            if (!hasNode(u))
                return;
            if (u == *it)
                --storedNumberOfSelfLoops;
            --inducedDegree[u];
            --degree;
        });
        m -= degree;
        n -= 1;
    }
}

GraphW InducedSubgraphView::Realize() const {
    return GraphTools::subgraphFromNodes(originalGraph, nodeSubset.begin(), nodeSubset.end());
}

std::vector<node> InducedSubgraphView::getNeighborsVector(node u, bool inEdges) const {
    std::vector<node> result;
    auto cb = [&](node v) {
        if (hasNode(v))
            result.push_back(v);
    };

    if (inEdges) {
        result.reserve(degreeIn(u));
        originalGraph.forInNeighborsOf(u, cb);
    } else {
        result.reserve(degree(u));
        originalGraph.forNeighborsOf(u, cb);
    }

    return result;
}

std::pair<std::vector<node>, std::vector<edgeweight>>
InducedSubgraphView::getNeighborsWithWeightsVector(node u, bool inEdges) const {
    std::vector<node> nodeVec;
    std::vector<edgeweight> weightVec;

    auto cb = [&](node v) {
        if (hasNode(v)) {
            nodeVec.push_back(v);
            weightVec.push_back(weight(u, v));
        }
    };

    if (inEdges) {
        nodeVec.reserve(degreeIn(u));
        weightVec.reserve(degreeIn(u));
        originalGraph.forInNeighborsOf(u, cb);
    } else {
        nodeVec.reserve(degree(u));
        weightVec.reserve(degree(u));
        originalGraph.forNeighborsOf(u, cb);
    }

    return {nodeVec, weightVec};
}

void InducedSubgraphView::forEdgesVirtualImpl(
    bool directed, bool weighted, bool hasEdgeIds,
    std::function<void(node, node, edgeweight, edgeid)> handle) const {
    for (node u : nodeSubset)
        forEdgesOf(u, handle);
}

void InducedSubgraphView::forEdgesOfVirtualImpl(
    node u, bool directed, bool weighted, bool hasEdgeIds,
    std::function<void(node, node, edgeweight, edgeid)> handle) const {
    originalGraph.forEdgesOf(u, [&](node v, edgeweight w, edgeid x) {
        if (hasEdge(u, v))
            handle(u, v, w, x);
    });
}

void InducedSubgraphView::forInEdgesVirtualImpl(
    node u, bool directed, bool weighted, bool hasEdgeIds,
    std::function<void(node, node, edgeweight, edgeid)> handle) const {
    originalGraph.forInEdgesOf(u, [&](node v, edgeweight w, edgeid x) {
        if (hasEdge(u, v))
            handle(u, v, w, x);
    });
}

double InducedSubgraphView::parallelSumForEdgesVirtualImpl(
    bool directed, bool weighted, bool hasEdgeIds,
    std::function<double(node, node, edgeweight, edgeid)> handle) const {
    return originalGraph.parallelSumForEdges([&](node u, node v, edgeweight w, edgeid x) {
        if (hasEdge(u, v))
            handle(u, v, w, x);
    });
}

} // namespace NetworKit
