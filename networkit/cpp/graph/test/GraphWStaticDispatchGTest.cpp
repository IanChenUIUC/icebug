/*
 * GraphWStaticDispatchGTest.cpp
 *
 *  Created on: 07.19.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 *
 * Asserts that GraphW's static-dispatch iteration (GraphIteratorMixin, reached
 * through a concrete GraphW) yields results identical to the base Graph's
 * virtual std::function path (reached through a Graph&). Any divergence would
 * silently change results for algorithms migrated to the static path.
 */

#include <algorithm>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#include <networkit/graph/GraphW.hpp>

namespace NetworKit {

using Edge4 = std::tuple<node, node, edgeweight, edgeid>;

class GraphWStaticDispatchGTest : public testing::TestWithParam<std::tuple<bool, bool>> {
protected:
    bool weighted() const { return std::get<0>(GetParam()); }
    bool directed() const { return std::get<1>(GetParam()); }

    // A small graph with distinct weights, an isolated node, and a deleted node
    // (deleted-node handling is a classic divergence point between the paths).
    GraphW makeGraph() const {
        GraphW G(6, weighted(), directed());
        G.addEdge(0, 1, weighted() ? 1.5 : defaultEdgeWeight);
        G.addEdge(0, 2, weighted() ? 2.5 : defaultEdgeWeight);
        G.addEdge(1, 2, weighted() ? 3.5 : defaultEdgeWeight);
        G.addEdge(2, 3, weighted() ? 4.5 : defaultEdgeWeight);
        G.addEdge(3, 4, weighted() ? 5.5 : defaultEdgeWeight);
        G.addEdge(1, 4, weighted() ? 6.5 : defaultEdgeWeight);
        G.removeNode(5); // isolated -> deleted
        G.addNode();     // isolated live node
        return G;
    }

    template <typename Coll>
    static std::vector<Edge4> sorted(Coll v) {
        std::sort(v.begin(), v.end());
        return v;
    }

    void checkEquivalence(const GraphW &G) const {
        const Graph &base = G; // routes through the virtual std::function path

        {
            std::vector<Edge4> viaVirtual, viaMixin;
            base.forEdges(
                [&](node u, node v, edgeweight w, edgeid e) { viaVirtual.emplace_back(u, v, w, e); });
            G.forEdges(
                [&](node u, node v, edgeweight w, edgeid e) { viaMixin.emplace_back(u, v, w, e); });
            EXPECT_EQ(viaVirtual, viaMixin) << "forEdges";
        }

        G.forNodes([&](node u) {
            std::vector<Edge4> ev, em;
            base.forEdgesOf(u,
                            [&](node a, node b, edgeweight w, edgeid e) { ev.emplace_back(a, b, w, e); });
            G.forEdgesOf(u,
                         [&](node a, node b, edgeweight w, edgeid e) { em.emplace_back(a, b, w, e); });
            EXPECT_EQ(ev, em) << "forEdgesOf u=" << u;

            std::vector<node> nv, nm;
            base.forNeighborsOf(u, [&](node v) { nv.push_back(v); });
            G.forNeighborsOf(u, [&](node v) { nm.push_back(v); });
            EXPECT_EQ(nv, nm) << "forNeighborsOf u=" << u;

            std::vector<Edge4> iev, iem;
            base.forInEdgesOf(
                u, [&](node a, node b, edgeweight w, edgeid e) { iev.emplace_back(a, b, w, e); });
            G.forInEdgesOf(
                u, [&](node a, node b, edgeweight w, edgeid e) { iem.emplace_back(a, b, w, e); });
            EXPECT_EQ(iev, iem) << "forInEdgesOf u=" << u;

            std::vector<node> inv, inm;
            base.forInNeighborsOf(u, [&](node v) { inv.push_back(v); });
            G.forInNeighborsOf(u, [&](node v) { inm.push_back(v); });
            EXPECT_EQ(inv, inm) << "forInNeighborsOf u=" << u;
        });

        // parallelForEdges is order-nondeterministic; compare as an edge set.
        {
            std::vector<Edge4> serial, par;
            G.forEdges(
                [&](node u, node v, edgeweight w, edgeid e) { serial.emplace_back(u, v, w, e); });
            G.parallelForEdges([&](node u, node v, edgeweight w, edgeid e) {
#pragma omp critical
                par.emplace_back(u, v, w, e);
            });
            EXPECT_EQ(sorted(serial), sorted(par)) << "parallelForEdges";
        }

        {
            double serial = 0.0;
            G.forEdges([&](node, node, edgeweight w, edgeid) { serial += w; });
            double viaMixin = G.parallelSumForEdges(
                [](node, node, edgeweight w, edgeid) -> double { return w; });
            EXPECT_NEAR(serial, viaMixin, 1e-9) << "parallelSumForEdges";
        }
    }
};

INSTANTIATE_TEST_SUITE_P(InstantiationName, GraphWStaticDispatchGTest,
                         testing::Values(std::make_tuple(false, false),
                                         std::make_tuple(true, false), std::make_tuple(false, true),
                                         std::make_tuple(true, true)));

TEST_P(GraphWStaticDispatchGTest, testMixinMatchesVirtual) {
    GraphW G = makeGraph();
    checkEquivalence(G);        // without edge ids
    G.indexEdges();
    checkEquivalence(G);        // with edge ids
}

} // namespace NetworKit
