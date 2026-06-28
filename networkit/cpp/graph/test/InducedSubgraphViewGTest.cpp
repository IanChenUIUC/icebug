/*
 * InducedSubgraphViewGTest.cpp
 *
 *  Created on: 06.26.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <algorithm>
#include <string>
#include <tuple>

#include "networkit/auxiliary/Random.hpp"
#include <gtest/gtest.h>

#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/NumericTools.hpp>
#include <networkit/auxiliary/Parallel.hpp>
#include <networkit/generators/ErdosRenyiGenerator.hpp>
#include <networkit/graph/Graph.hpp>
#include <networkit/graph/GraphBuilder.hpp>
#include <networkit/graph/GraphTools.hpp>
#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/io/METISGraphReader.hpp>

namespace NetworKit {

class InducedSubgraphViewGTest : public testing::TestWithParam<std::tuple<bool, bool>> {
public:
    virtual void SetUp();

protected:
    GraphW Ghouse;
    std::vector<std::pair<node, node>> houseEdgesOut;
    std::vector<std::vector<edgeweight>> Ahouse;
    count n_house;
    count m_house;

    bool isInducedSubgraphView() const { return !isWeighted() && !isDirected(); }
    bool isWeightedInducedSubgraphView() const { return isWeighted() && !isDirected(); }
    bool isDirectedInducedSubgraphView() const { return !isWeighted() && isDirected(); }
    bool isWeightedDirectedInducedSubgraphView() const { return isWeighted() && isDirected(); }

    bool isWeighted() const;
    bool isDirected() const;
    GraphW createGraphW(count n = 0) const;
    GraphW createGraphW(count n, count m) const;
    std::set<node> allNodes(const Graph &G) const;
    std::set<node> someNodes(const Graph &G, count k, bool random = false) const;

    struct NodeSubset {
        std::string label;
        std::set<node> nodes;
    };
    std::vector<NodeSubset> subsetVariants(const Graph &G) const;
    GraphW inducedSubgraph(const Graph &G, const std::set<node> &subset) const;

    count countSelfLoopsManually(const InducedSubgraphView &G);
};

INSTANTIATE_TEST_SUITE_P(InstantiationName, InducedSubgraphViewGTest,
                         testing::Values(std::make_tuple(false, false),
                                         std::make_tuple(true, false), std::make_tuple(false, true),
                                         std::make_tuple(true, true)));

bool InducedSubgraphViewGTest::isWeighted() const {
    return std::get<0>(GetParam());
}
bool InducedSubgraphViewGTest::isDirected() const {
    return std::get<1>(GetParam());
}

GraphW InducedSubgraphViewGTest::createGraphW(count n) const {
    bool weighted, directed;
    std::tie(weighted, directed) = GetParam();
    GraphW G(n, weighted, directed);
    return G;
}

GraphW InducedSubgraphViewGTest::createGraphW(count n, count m) const {
    GraphW G = createGraphW(n);
    while (G.numberOfEdges() < m) {
        const auto u = Aux::Random::index(n);
        const auto v = Aux::Random::index(n);
        if (u == v)
            continue;
        if (G.hasEdge(u, v))
            continue;

        const auto p = Aux::Random::probability();
        G.addEdge(u, v, p);
    }
    return G;
}

std::set<node> InducedSubgraphViewGTest::allNodes(const Graph &G) const {
    std::set<node> subset;
    G.forNodes([&](node u) { subset.insert(u); });
    return subset;
}

std::set<node> InducedSubgraphViewGTest::someNodes(const Graph &G, count k, bool random) const {
    std::set<node> subset;
    index idx = 0;
    auto cb = [&](node u) {
        if (idx++ < k)
            subset.insert(u);
    };
    if (random)
        G.forNodesInRandomOrder(cb);
    else
        G.forNodes(cb);
    return subset;
}

std::vector<InducedSubgraphViewGTest::NodeSubset>
InducedSubgraphViewGTest::subsetVariants(const Graph &G) const {
    const count n = G.numberOfNodes();
    const count small = std::max<count>(2, static_cast<count>(0.1 * n));
    const count half = (n + 1) / 2;
    return {{"small", someNodes(G, small, true)},
            {"half", someNodes(G, half, true)},
            {"all", allNodes(G)}};
}

GraphW InducedSubgraphViewGTest::inducedSubgraph(const Graph &G,
                                                 const std::set<node> &subset) const {
    return GraphTools::subgraphFromNodes(G, subset.begin(), subset.end());
}

count InducedSubgraphViewGTest::countSelfLoopsManually(const InducedSubgraphView &G) {
    count c = 0;
    G.getOriginalGraph().forEdges([&](node u, node v) {
        if (u == v && G.getNodeSubset().contains(u)) {
            c += 1;
        }
    });
    return c;
}

void InducedSubgraphViewGTest::SetUp() {
    /*
     *    0
     *   . \
     *  /   \
     * /     .
     * 1 <-- 2
     * ^ \  .|
     * |  \/ |
     * | / \ |
     * |/   ..
     * 3 <-- 4
     *
     * move you pen from node to node:
     * 3 -> 1 -> 0 -> 2 -> 1 -> 4 -> 3 -> 2 -> 4
     */
    n_house = 5;
    m_house = 8;

    Ghouse = createGraphW(5);
    houseEdgesOut = {{3, 1}, {1, 0}, {0, 2}, {2, 1}, {1, 4}, {4, 3}, {3, 2}, {2, 4}};
    Ahouse = {n_house, std::vector<edgeweight>(n_house, 0.0)};
    edgeweight ew = 1.0;
    for (auto &e : houseEdgesOut) {
        node u = e.first;
        node v = e.second;
        Ghouse.addEdge(u, v, ew);

        Ahouse[u][v] = ew;

        if (!Ghouse.isDirected()) {
            Ahouse[v][u] = ew;
        }

        if (Ghouse.isWeighted()) {
            ew += 1.0;
        }
    }
}

/** CONSTRUCTORS **/

// TODO

/** NODE MODIFIERS **/

TEST_P(InducedSubgraphViewGTest, testAddNode) {
    GraphW H = createGraphW(10, 25);

    InducedSubgraphView G(H, std::set<node>{});
    ASSERT_FALSE(G.hasNode(0));
    ASSERT_FALSE(G.hasNode(1));
    ASSERT_EQ(0u, G.numberOfNodes());

    G.addNode(0);
    ASSERT_TRUE(G.hasNode(0));
    ASSERT_FALSE(G.hasNode(1));
    ASSERT_EQ(1u, G.numberOfNodes());

    InducedSubgraphView G2(H, someNodes(H, 2));
    ASSERT_TRUE(G2.hasNode(0));
    ASSERT_TRUE(G2.hasNode(1));
    ASSERT_FALSE(G2.hasNode(2));
    ASSERT_EQ(2u, G2.numberOfNodes());

    G2.addNode(2);
    G2.addNode(3);
    ASSERT_TRUE(G2.hasNode(2));
    ASSERT_TRUE(G2.hasNode(3));
    ASSERT_FALSE(G2.hasNode(4));
    ASSERT_EQ(4u, G2.numberOfNodes());
}

TEST_P(InducedSubgraphViewGTest, testAddNodes) {
    GraphW H = createGraphW(100);

    InducedSubgraphView G(H, someNodes(H, 5));
    ASSERT_EQ(G.numberOfNodes(), 5u);

    std::set<node> batch1;
    for (node u = 5; u < 10; ++u)
        batch1.insert(u);
    G.addNodes(batch1);
    ASSERT_EQ(G.numberOfNodes(), 10u);
    ASSERT_TRUE(G.hasNode(9));

    std::set<node> batch2;
    for (node u = 10; u < 100; ++u)
        batch2.insert(u);
    G.addNodes(batch2);
    ASSERT_EQ(G.numberOfNodes(), 100u);
    ASSERT_TRUE(G.hasNode(99));
}

TEST_P(InducedSubgraphViewGTest, testRemoveNode) {
    auto testInducedSubgraphView = [&](const GraphW &base) {
        InducedSubgraphView G(base, allNodes(base));

        count n = G.numberOfNodes();
        count m = G.numberOfEdges();
        const count z = base.upperNodeIdBound();
        for (node u = 0; u < z; ++u) {
            const count deg = G.degreeOut(u);
            const count degIn = G.isDirected() ? G.degreeIn(u) : 0;
            G.removeNodes({u});
            --n;
            m -= deg + degIn;
            ASSERT_EQ(G.numberOfNodes(), n);
            ASSERT_EQ(G.numberOfEdges(), m);
            base.forNodes([&](node v) { ASSERT_EQ(G.hasNode(v), v > u); });
        }
    };

    GraphW G1 = ErdosRenyiGenerator(200, 0.2, false).generate();
    GraphW G2 = ErdosRenyiGenerator(200, 0.2, true).generate();
    testInducedSubgraphView(G1);
    testInducedSubgraphView(G2);
}

TEST_P(InducedSubgraphViewGTest, testHasNode) {
    GraphW H = createGraphW(7);
    InducedSubgraphView G(H, someNodes(H, 5));

    ASSERT_TRUE(G.hasNode(0));
    ASSERT_TRUE(G.hasNode(1));
    ASSERT_TRUE(G.hasNode(2));
    ASSERT_TRUE(G.hasNode(3));
    ASSERT_TRUE(G.hasNode(4));
    ASSERT_FALSE(G.hasNode(5));
    ASSERT_FALSE(G.hasNode(6));

    G.removeNodes({0, 2});
    G.addNodes({5});

    ASSERT_FALSE(G.hasNode(0));
    ASSERT_TRUE(G.hasNode(1));
    ASSERT_FALSE(G.hasNode(2));
    ASSERT_TRUE(G.hasNode(3));
    ASSERT_TRUE(G.hasNode(4));
    ASSERT_TRUE(G.hasNode(5));
    ASSERT_FALSE(G.hasNode(6));
}

TEST_P(InducedSubgraphViewGTest, testAddRemoveNodesOutsideOriginalGraph) {
    GraphW H = createGraphW(5);
    InducedSubgraphView G(H, allNodes(H));
    ASSERT_EQ(5u, G.numberOfNodes());

    G.addNodes({10, 11, 12});
    ASSERT_EQ(5u, G.numberOfNodes());
    ASSERT_FALSE(G.hasNode(10));
    ASSERT_FALSE(G.hasNode(11));
    ASSERT_FALSE(G.hasNode(12));

    G.removeNodes({3, 4});
    ASSERT_EQ(3u, G.numberOfNodes());

    G.removeNodes({3, 4, 42});
    ASSERT_EQ(3u, G.numberOfNodes());
    ASSERT_TRUE(G.hasNode(0));
    ASSERT_TRUE(G.hasNode(1));
    ASSERT_TRUE(G.hasNode(2));
}

/** NODE PROPERTIES **/

TEST_P(InducedSubgraphViewGTest, testDegree) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes)
            EXPECT_EQ(expected.degree(u), Gv.degree(u));
    }
}

TEST_P(InducedSubgraphViewGTest, testDegreeIn) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes)
            EXPECT_EQ(expected.degreeIn(u), Gv.degreeIn(u));
    }
}

TEST_P(InducedSubgraphViewGTest, testDegreeOut) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes)
            EXPECT_EQ(expected.degreeOut(u), Gv.degreeOut(u));
    }
}

TEST_P(InducedSubgraphViewGTest, testWeightedDegree) {
    this->Ghouse.addEdge(2, 2, 0.75);

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes)
            EXPECT_EQ(expected.weightedDegree(u), Gv.weightedDegree(u));
    }
}

TEST_P(InducedSubgraphViewGTest, testWeightedDegree2) {
    this->Ghouse.addEdge(2, 2, 0.75);

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes)
            EXPECT_EQ(expected.weightedDegree(u, true), Gv.weightedDegree(u, true));
    }
}

TEST_P(InducedSubgraphViewGTest, testWeightedDegree3) {
    constexpr count n = 100;
    constexpr double p = 0.1;

    for (int seed : {1, 2, 3}) {
        Aux::Random::setSeed(seed, false);
        auto base = ErdosRenyiGenerator(n, p, isDirected()).generate();
        if (isWeighted()) {
            GraphTools::randomizeWeights(base);
        }
        for (const auto &variant : subsetVariants(base)) {
            SCOPED_TRACE(variant.label);
            InducedSubgraphView G(base, variant.nodes);
            GraphW expected = inducedSubgraph(base, variant.nodes);
            for (node u : variant.nodes) {
                EXPECT_DOUBLE_EQ(expected.weightedDegree(u), G.weightedDegree(u));
                EXPECT_DOUBLE_EQ(expected.weightedDegree(u, true), G.weightedDegree(u, true));
                EXPECT_DOUBLE_EQ(expected.weightedDegreeIn(u), G.weightedDegreeIn(u));
                EXPECT_DOUBLE_EQ(expected.weightedDegreeIn(u, true), G.weightedDegreeIn(u, true));
            }
        }
    }
}
/*
 * ----------------------------------------------------------------------------
 * Conversion in progress. Remaining GraphW-derived tests are not yet translated.
 *
 * Tests intentionally DROPPED (a read-only view cannot exercise them):
 *   - graph mutation: testRestoreNode, testIsIsolated, testAddEdge, testRemoveEdge,
 *     testRemoveAllEdges, testRemoveSelfLoops, testRemoveMultiEdges, testSetWeight,
 *     increaseWeight, testSelfLoopConversion, testCheckConsistency_MultiEdgeDetection
 *   - edge ids / indexed graphs: testEdgeIndexGeneration*, testEdgeIndexResolver,
 *     testFor*EdgesWithIds, testParallelFor*EdgesWithIds, testSortEdges, testEdgeIds*
 *   - neighbor reordering: all testSortNeighbors*
 * ----------------------------------------------------------------------------
 */

} /* namespace NetworKit */
