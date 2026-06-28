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
    addNodes(subset);
}

std::set<node> InducedSubgraphView::boundary() {
    std::set<node> result;
    for (node u : nodeSubset) {
        originalGraph.get().forNeighborsOf(u, [&](node v) {
            if (!hasNode(v))
                result.insert(v);
        });
    }
    return result;
}

count InducedSubgraphView::degree(node v) const {
    assert(hasNode(v));
    return inducedDegree.at(v);
}

count InducedSubgraphView::degreeIn(node v) const {
    assert(hasNode(v));
    return isDirected() ? inducedInDegree.at(v) : degree(v);
};

edgeweight InducedSubgraphView ::weightedDegree(node u, bool countSelfLoopsTwice) const {
    assert(hasNode(u));
    return computeWeightedDegree(u, false, countSelfLoopsTwice);
}

edgeweight InducedSubgraphView ::weightedDegreeIn(node u, bool countSelfLoopsTwice) const {
    assert(hasNode(u));
    return computeWeightedDegree(u, true, countSelfLoopsTwice);
}

index InducedSubgraphView::upperNodeIdBound() const noexcept {
    return originalGraph.get().upperNodeIdBound();
}

bool InducedSubgraphView::isIsolated(node v) const {
    assert(hasNode(v));
    return degree(v) == 0 && (!directed || degreeIn(v) == 0);
}

bool InducedSubgraphView::hasNode(node v) const noexcept {
    return nodeSubset.contains(v);
}

bool InducedSubgraphView::hasEdge(node u, node v) const noexcept {
    return hasNode(u) && hasNode(v) && originalGraph.get().hasEdge(u, v);
}

edgeweight InducedSubgraphView::weight(node u, node v) const {
    if (!hasEdge(u, v))
        return 0;
    return originalGraph.get().weight(u, v);
}

void InducedSubgraphView::addNodes(const std::set<node> &subset) {
    for (node v : subset) {
        if (hasNode(v) || !originalGraph.get().hasNode(v))
            continue;

        nodeSubset.insert(v);
        n += 1;

        count degree = 0;
        originalGraph.get().forNeighborsOf(v, [&](node u, edgeweight ew) {
            if (!hasNode(u))
                return;
            if (u == v)
                ++storedNumberOfSelfLoops;
            isDirected() ? ++inducedInDegree[u] : ++inducedDegree[u];
            ++degree;
        });

        inducedDegree[v] = degree;
        m += degree;

        if (isDirected()) {
            count inDegree = 0;
            originalGraph.get().forInNeighborsOf(v, [&](node u, edgeweight ew) {
                if (!hasNode(u) || u == v)
                    return;
                ++inducedDegree[u];
                ++inDegree;
            });
            inducedInDegree[v] = inDegree;
            m += inDegree;
        }
    }
}

void InducedSubgraphView::removeNodes(const std::set<node> &subset) {
    for (node v : subset) {
        if (!hasNode(v))
            continue;

        n -= 1;
        m -= inducedDegree[v];
        if (isDirected())
            m -= inducedInDegree[v];

        originalGraph.get().forNeighborsOf(v, [&](node u, edgeweight ew) {
            if (!hasNode(u))
                return;
            if (u == v)
                --storedNumberOfSelfLoops;
            isDirected() ? --inducedInDegree[u] : --inducedDegree[u];
        });

        if (isDirected()) {
            originalGraph.get().forInNeighborsOf(v, [&](node u, edgeweight ew) {
                if (!hasNode(u) || u == v)
                    return;
                --inducedDegree[u];
            });
        }

        nodeSubset.erase(v);
        inducedDegree.erase(v);
        if (isDirected())
            inducedInDegree.erase(v);
    }
}

GraphW InducedSubgraphView::realize(bool compact) const {
    return GraphTools::subgraphFromNodes(originalGraph, nodeSubset.begin(), nodeSubset.end(),
                                         compact);
}

std::vector<node> InducedSubgraphView::getNeighborsVector(node u, bool inEdges) const {
    std::vector<node> result;
    auto cb = [&](node v) {
        if (hasNode(v))
            result.push_back(v);
    };

    if (inEdges) {
        result.reserve(degreeIn(u));
        originalGraph.get().forInNeighborsOf(u, cb);
    } else {
        result.reserve(degree(u));
        originalGraph.get().forNeighborsOf(u, cb);
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
            if (inEdges)
                weightVec.push_back(weight(v, u));
            else
                weightVec.push_back(weight(u, v));
        }
    };

    if (inEdges) {
        nodeVec.reserve(degreeIn(u));
        weightVec.reserve(degreeIn(u));
        originalGraph.get().forInNeighborsOf(u, cb);
    } else {
        nodeVec.reserve(degree(u));
        weightVec.reserve(degree(u));
        originalGraph.get().forNeighborsOf(u, cb);
    }

    return {nodeVec, weightVec};
}

void InducedSubgraphView::forEdgesVirtualImpl(
    bool directed, bool weighted, bool hasEdgeIds,
    std::function<void(node, node, edgeweight, edgeid)> handle) const {
    originalGraph.get().forEdges([&](node u, node v, edgeweight w, edgeid x) {
        if (hasEdge(u, v))
            handle(u, v, w, x);
    });
}

void InducedSubgraphView::forEdgesOfVirtualImpl(
    node u, bool directed, bool weighted, bool hasEdgeIds,
    std::function<void(node, node, edgeweight, edgeid)> handle) const {
    originalGraph.get().forEdgesOf(u, [&](node u, node v, edgeweight w, edgeid x) {
        if (hasNode(v))
            handle(u, v, w, x);
    });
}

void InducedSubgraphView::forInEdgesOfVirtualImpl(
    node u, bool directed, bool weighted, bool hasEdgeIds,
    std::function<void(node, node, edgeweight, edgeid)> handle) const {
    originalGraph.get().forInEdgesOf(u, [&](node u, node v, edgeweight w, edgeid x) {
        if (hasNode(v))
            handle(v, u, w, x);
    });
}

double InducedSubgraphView::parallelSumForEdgesVirtualImpl(
    bool directed, bool weighted, bool hasEdgeIds,
    std::function<double(node, node, edgeweight, edgeid)> handle) const {
    return originalGraph.get().parallelSumForEdges([&](node u, node v, edgeweight w, edgeid x) {
        return hasEdge(u, v) ? handle(u, v, w, x) : 0;
    });
}

edgeid InducedSubgraphView::edgeId(node u, node v) const {
    throw std::runtime_error("edgeId not supported for graph view; realize() into GraphW");
}
node InducedSubgraphView::getIthNeighbor(Unsafe, node u, index i) const {
    throw std::runtime_error("getIthNeighbor not supported for graph view; realize() into GraphW");
}
edgeweight InducedSubgraphView::getIthNeighborWeight(node u, index i) const {
    throw std::runtime_error(
        "getIthNeighborWeight not supported for graph view; realize() into GraphW");
}
node InducedSubgraphView::getIthNeighbor(node u, index i) const {
    throw std::runtime_error("getIthNeighbor not supported for graph view; realize() into GraphW");
}
node InducedSubgraphView::getIthInNeighbor(node u, index i) const {
    throw std::runtime_error(
        "getIthInNeighbor not supported for graph view; realize() into GraphW");
}
std::pair<node, edgeweight> InducedSubgraphView::getIthNeighborWithWeight(node u, index i) const {
    throw std::runtime_error(
        "getIthNeighborWithWeight not supported for graph view; realize() into GraphW");
}
std::pair<node, edgeid> InducedSubgraphView::getIthNeighborWithId(node u, index i) const {
    throw std::runtime_error(
        "getIthNeighborWithId not supported for graph view; realize() into GraphW");
}
index InducedSubgraphView::indexInInEdgeArray(node v, node u) const {
    throw std::runtime_error(
        "indexInInEdgeArray not supported for graph view; realize() into GraphW");
}
index InducedSubgraphView::indexInOutEdgeArray(node u, node v) const {
    throw std::runtime_error(
        "indexInOutEdgeArray not supported for graph view; realize() into GraphW");
}

edgeweight InducedSubgraphView::computeWeightedDegree(node u, bool inDegree,
                                                      bool countSelfLoopsTwice) const {
    if (weighted) {
        edgeweight sum = 0.0;
        auto sumWeights = [&](node v, edgeweight w) {
            sum += (countSelfLoopsTwice && u == v) ? 2. * w : w;
        };
        if (inDegree) {
            forInNeighborsOf(u, sumWeights);
        } else {
            forNeighborsOf(u, sumWeights);
        }
        return sum;
    }

    count sum = inDegree ? degreeIn(u) : degreeOut(u);
    auto countSelfLoops = [&](node v) { sum += (u == v); };

    if (countSelfLoopsTwice && numberOfSelfLoops()) {
        if (inDegree) {
            forInNeighborsOf(u, countSelfLoops);
        } else {
            forNeighborsOf(u, countSelfLoops);
        }
    }

    return static_cast<edgeweight>(sum);
}

} // namespace NetworKit
