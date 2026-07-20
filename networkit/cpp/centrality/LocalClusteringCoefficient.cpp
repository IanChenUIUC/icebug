#include <omp.h>
#include <networkit/centrality/LocalClusteringCoefficient.hpp>
#include <networkit/graph/GraphDispatch.hpp>

namespace NetworKit {

LocalClusteringCoefficient::LocalClusteringCoefficient(const Graph &G, bool turbo)
    : Centrality(G, false, false), turbo(turbo) {
    if (G.isDirected())
        throw std::runtime_error("Not implemented: Local clustering coefficient is currently not "
                                 "implemented for directed graphs");
    if (G.numberOfSelfLoops())
        throw std::runtime_error("Local Clustering Coefficient implementation does not support "
                                 "graphs with self-loops. Call Graph.removeSelfLoops() first.");
}

void LocalClusteringCoefficient::run() {
    visitConcreteGraph(G, [&](const auto &g) { runImpl(g); });
    hasRun = true;
}

template <class Gr>
void LocalClusteringCoefficient::runImpl(const Gr &g) {
    count z = g.upperNodeIdBound();
    scoreData.clear();
    scoreData.resize(z); // $c(u) := \frac{2 \cdot |E(N(u))| }{\deg(u) \cdot ( \deg(u) - 1)}$

    std::vector<index> inBegin;
    std::vector<node> inEdges;

    if (turbo) {
        auto isOutEdge = [&](node u, node v) {
            return g.degree(u) > g.degree(v) || (g.degree(u) == g.degree(v) && u < v);
        };

        inBegin.resize(g.upperNodeIdBound() + 1);
        inEdges.resize(g.numberOfEdges());
        index pos = 0;
        for (index u = 0; u < g.upperNodeIdBound(); ++u) {
            inBegin[u] = pos;
            if (g.hasNode(u)) {
                g.forEdgesOf(u, [&](node v) {
                    if (isOutEdge(v, u)) {
                        inEdges[pos++] = v;
                    }
                });
            }
        }
        inBegin[g.upperNodeIdBound()] = pos;
    }

    std::vector<std::vector<bool>> nodeMarker(omp_get_max_threads());

    for (auto &nm : nodeMarker) {
        nm.resize(z, false);
    }

    g.balancedParallelForNodes([&](node u) {
        count d = g.degree(u);

        if (d < 2) {
            scoreData[u] = 0.0;
        } else {
            size_t tid = omp_get_thread_num();
            count triangles = 0;

            g.forEdgesOf(u, [&](node v) { nodeMarker[tid][v] = true; });

            g.forEdgesOf(u, [&](node, node v) {
                if (turbo) {
                    for (index i = inBegin[v]; i < inBegin[v + 1]; ++i) {
                        node w = inEdges[i];
                        if (nodeMarker[tid][w]) {
                            triangles += 1;
                        }
                    }
                } else {
                    g.forEdgesOf(v, [&](node, node w) {
                        if (nodeMarker[tid][w]) {
                            triangles += 1;
                        }
                    });
                }
            });

            g.forEdgesOf(u, [&](node, node v) { nodeMarker[tid][v] = false; });

            // No division by 2 since triangles are counted twice as well!
            scoreData[u] = (double)triangles / (double)(d * (d - 1));
            if (turbo)
                scoreData[u] *= 2; // in turbo mode, we count each triangle only once
        }
    });
}

template void LocalClusteringCoefficient::runImpl<Graph>(const Graph &);
template void LocalClusteringCoefficient::runImpl<GraphW>(const GraphW &);

double LocalClusteringCoefficient::maximum() {
    return 1.0;
}

} // namespace NetworKit
