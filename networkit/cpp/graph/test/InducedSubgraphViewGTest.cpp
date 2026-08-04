/*
 * InducedSubgraphViewGTest.cpp
 *
 *  Created on: 06.26.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <algorithm>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include <networkit/auxiliary/Random.hpp>

#include <networkit/auxiliary/Log.hpp>
#include <networkit/auxiliary/NumericTools.hpp>
#include <networkit/auxiliary/Parallel.hpp>
#include <networkit/centrality/CoreDecomposition.hpp>
#include <networkit/generators/ErdosRenyiGenerator.hpp>
#include <networkit/graph/Graph.hpp>
#include <networkit/graph/GraphBuilder.hpp>
#include <networkit/graph/GraphTools.hpp>
#include <networkit/graph/InducedSubgraphView.hpp>
#include <networkit/graph/InducedSubgraphViewImpl.hpp>
#include <networkit/io/METISGraphReader.hpp>

namespace NetworKit {

class InducedSubgraphViewGTest : public testing::TestWithParam<std::tuple<bool, bool, bool>> {
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
    bool isCompact() const;

    InducedSubgraphView makeView(const GraphW &base, std::vector<node> subset) const {
        std::sort(subset.begin(), subset.end());
        subset.erase(std::unique(subset.begin(), subset.end()), subset.end());
        InducedSubgraphView Gv(base, isCompact());
        Gv.addNodes(Aux::sorted_unique, std::span<const node>(subset));
        return Gv;
    }

    GraphW reference(const Graph &base, const std::vector<node> &subset) const {
        return GraphTools::subgraphFromNodes(base, subset.begin(), subset.end(), isCompact());
    }

    GraphW createGraphW(count n = 0) const;
    GraphW createGraphW(count n, count m) const;
    std::vector<node> allNodes(const Graph &G) const;
    std::vector<node> someNodes(const Graph &G, count k, bool random = false) const;

    struct NodeSubset {
        std::string label;
        std::vector<node> nodes;
    };
    std::vector<NodeSubset> subsetVariants(const Graph &G) const;
    GraphW inducedSubgraph(const Graph &G, const std::vector<node> &subset) const;
    void expectGraphsEqual(const Graph &expected, const Graph &actual) const;
};

INSTANTIATE_TEST_SUITE_P(InstantiationName, InducedSubgraphViewGTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

bool InducedSubgraphViewGTest::isWeighted() const {
    return std::get<0>(GetParam());
}
bool InducedSubgraphViewGTest::isDirected() const {
    return std::get<1>(GetParam());
}
bool InducedSubgraphViewGTest::isCompact() const {
    return std::get<2>(GetParam());
}

GraphW InducedSubgraphViewGTest::createGraphW(count n) const {
    GraphW G(n, isWeighted(), isDirected());
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

std::vector<node> InducedSubgraphViewGTest::allNodes(const Graph &G) const {
    std::vector<node> subset;
    G.forNodes([&](node u) { subset.push_back(u); });
    return subset;
}

std::vector<node> InducedSubgraphViewGTest::someNodes(const Graph &G, count k, bool random) const {
    std::vector<node> subset;
    index idx = 0;
    auto cb = [&](node u) {
        if (idx++ < k)
            subset.push_back(u);
    };
    if (random)
        G.forNodesInRandomOrder(cb);
    else
        G.forNodes(cb);
    std::sort(subset.begin(), subset.end());
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
                                                 const std::vector<node> &subset) const {
    return GraphTools::subgraphFromNodes(G, subset.begin(), subset.end());
}

void InducedSubgraphViewGTest::expectGraphsEqual(const Graph &expected, const Graph &actual) const {
    EXPECT_EQ(expected.isWeighted(), actual.isWeighted());
    EXPECT_EQ(expected.isDirected(), actual.isDirected());
    EXPECT_EQ(expected.numberOfNodes(), actual.numberOfNodes());
    EXPECT_EQ(expected.numberOfEdges(), actual.numberOfEdges());
    EXPECT_EQ(expected.upperNodeIdBound(), actual.upperNodeIdBound());

    std::set<node> expectedNodes, actualNodes;
    expected.forNodes([&](node u) { expectedNodes.insert(u); });
    actual.forNodes([&](node u) { actualNodes.insert(u); });
    EXPECT_EQ(expectedNodes, actualNodes);

    auto edges = [](const Graph &g) {
        std::multiset<std::tuple<node, node, edgeweight>> s;
        g.forEdges([&](node u, node v, edgeweight w) {
            node a = g.isDirected() ? u : std::min(u, v);
            node b = g.isDirected() ? v : std::max(u, v);
            s.insert({a, b, w});
        });
        return s;
    };
    EXPECT_EQ(edges(expected), edges(actual));
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

/** VIEW OPERATIONS **/

TEST_P(InducedSubgraphViewGTest, testRealize) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, false);
        Gv.addNodes(variant.nodes);

        GraphW expectedKeep = inducedSubgraph(Ghouse, variant.nodes);
        expectGraphsEqual(expectedKeep, Gv.realize());
        expectGraphsEqual(expectedKeep, Gv.realize(false));

        GraphW expectedCompact =
            GraphTools::subgraphFromNodes(Ghouse, variant.nodes.begin(), variant.nodes.end(), true);
        GraphW compact = Gv.realize(true);
        expectGraphsEqual(expectedCompact, compact);
        EXPECT_EQ(variant.nodes.size(), compact.numberOfNodes());
        EXPECT_EQ(variant.nodes.size(), compact.upperNodeIdBound());
    }
}

TEST_P(InducedSubgraphViewGTest, testCopyAndMove) {
    InducedSubgraphView original(Ghouse, false);
    original.addNodes(someNodes(Ghouse, 3));

    InducedSubgraphView copy(original);
    EXPECT_EQ(original.getNodeSubset(), copy.getNodeSubset());
    expectGraphsEqual(original.realize(), copy.realize());

    const count originalCount = original.numberOfNodes();
    copy.removeNodes({{copy.getNodeSubset().front()}});
    EXPECT_EQ(originalCount, original.numberOfNodes());
    EXPECT_NE(original.getNodeSubset(), copy.getNodeSubset());

    InducedSubgraphView source(Ghouse, false);
    source.addNodes(someNodes(Ghouse, 4));
    std::vector<node> expectedSubset = source.getNodeSubset();
    InducedSubgraphView moved(std::move(source));
    EXPECT_EQ(expectedSubset, moved.getNodeSubset());
    expectGraphsEqual(inducedSubgraph(Ghouse, expectedSubset), moved.realize());
}

/** NODE MODIFIERS **/

TEST_P(InducedSubgraphViewGTest, testAddNode) {
    if (isCompact())
        GTEST_SKIP() << "queries specific global IDs; sparse-mode semantics";
    GraphW H = createGraphW(10, 25);

    InducedSubgraphView G(H, false);
    ASSERT_FALSE(G.hasNode(0));
    ASSERT_FALSE(G.hasNode(1));
    ASSERT_EQ(0u, G.numberOfNodes());

    G.addNode(0);
    ASSERT_TRUE(G.hasNode(0));
    ASSERT_FALSE(G.hasNode(1));
    ASSERT_EQ(1u, G.numberOfNodes());

    InducedSubgraphView G2(H, false);
    G2.addNodes(someNodes(H, 2));
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
    if (isCompact())
        GTEST_SKIP() << "queries specific global IDs; sparse-mode semantics";
    GraphW H = createGraphW(100);

    InducedSubgraphView G(H, false);
    G.addNodes(someNodes(H, 5));
    ASSERT_EQ(G.numberOfNodes(), 5u);

    std::vector<node> batch1;
    for (node u = 5; u < 10; ++u)
        batch1.push_back(u);
    G.addNodes(batch1);
    ASSERT_EQ(G.numberOfNodes(), 10u);
    ASSERT_TRUE(G.hasNode(9));

    std::vector<node> batch2;
    for (node u = 10; u < 100; ++u)
        batch2.push_back(u);
    G.addNodes(batch2);
    ASSERT_EQ(G.numberOfNodes(), 100u);
    ASSERT_TRUE(G.hasNode(99));
}

TEST_P(InducedSubgraphViewGTest, testRemoveNode) {
    if (isCompact())
        GTEST_SKIP() << "removes specific global IDs and checks global hasNode; sparse-only";
    auto testInducedSubgraphView = [&](const GraphW &base) {
        InducedSubgraphView G(base, false);
        G.addNodes(allNodes(base));

        count n = G.numberOfNodes();
        count m = G.numberOfEdges();
        const count z = base.upperNodeIdBound();
        for (node u = 0; u < z; ++u) {
            const count deg = G.degreeOut(u);
            const count degIn = G.isDirected() ? G.degreeIn(u) : 0;
            G.removeNodes({{u}});
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
    if (isCompact())
        GTEST_SKIP() << "queries specific global IDs; sparse-mode semantics";
    GraphW H = createGraphW(7);
    InducedSubgraphView G(H, false);
    G.addNodes(someNodes(H, 5));

    ASSERT_TRUE(G.hasNode(0));
    ASSERT_TRUE(G.hasNode(1));
    ASSERT_TRUE(G.hasNode(2));
    ASSERT_TRUE(G.hasNode(3));
    ASSERT_TRUE(G.hasNode(4));
    ASSERT_FALSE(G.hasNode(5));
    ASSERT_FALSE(G.hasNode(6));

    G.removeNodes({{0, 2}});
    G.addNodes({{5}});

    ASSERT_FALSE(G.hasNode(0));
    ASSERT_TRUE(G.hasNode(1));
    ASSERT_FALSE(G.hasNode(2));
    ASSERT_TRUE(G.hasNode(3));
    ASSERT_TRUE(G.hasNode(4));
    ASSERT_TRUE(G.hasNode(5));
    ASSERT_FALSE(G.hasNode(6));
}

TEST_P(InducedSubgraphViewGTest, testAddRemoveNodesOutsideOriginalGraph) {
    if (isCompact())
        GTEST_SKIP() << "queries specific global IDs; sparse-mode semantics";
    GraphW H = createGraphW(5);
    InducedSubgraphView G(H, false);
    G.addNodes(allNodes(H));
    ASSERT_EQ(5u, G.numberOfNodes());

    G.addNodes({{10, 11, 12}});
    ASSERT_EQ(5u, G.numberOfNodes());
    ASSERT_FALSE(G.hasNode(10));
    ASSERT_FALSE(G.hasNode(11));
    ASSERT_FALSE(G.hasNode(12));

    G.removeNodes({{3, 4}});
    ASSERT_EQ(3u, G.numberOfNodes());

    G.removeNodes({{3, 4, 42}});
    ASSERT_EQ(3u, G.numberOfNodes());
    ASSERT_TRUE(G.hasNode(0));
    ASSERT_TRUE(G.hasNode(1));
    ASSERT_TRUE(G.hasNode(2));
}

/** NODE PROPERTIES **/

TEST_P(InducedSubgraphViewGTest, testDegree) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) { EXPECT_EQ(expected.degree(u), Gv.degree(u)); });
    }
}

TEST_P(InducedSubgraphViewGTest, testDegreeIn) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) { EXPECT_EQ(expected.degreeIn(u), Gv.degreeIn(u)); });
    }
}

TEST_P(InducedSubgraphViewGTest, testDegreeOut) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) { EXPECT_EQ(expected.degreeOut(u), Gv.degreeOut(u)); });
    }
}

TEST_P(InducedSubgraphViewGTest, testWeightedDegree) {
    this->Ghouse.addEdge(2, 2, 0.75);

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes(
            [&](node u) { EXPECT_EQ(expected.weightedDegree(u), Gv.weightedDegree(u)); });
    }
}

TEST_P(InducedSubgraphViewGTest, testWeightedDegree2) {
    this->Ghouse.addEdge(2, 2, 0.75);

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            EXPECT_EQ(expected.weightedDegree(u, true), Gv.weightedDegree(u, true));
        });
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
            InducedSubgraphView G = makeView(base, variant.nodes);
            GraphW expected = reference(base, variant.nodes);
            /*
             * Compared with a tolerance, not exactly: the two graphs hold the same incident
             * weights but enumerate them in different orders -- the materialized subgraph in the
             * hash order subgraphFromNodes builds it in, the view in the base graph's storage
             * order -- and floating-point addition is not associative.
             */
            expected.forNodes([&](node u) {
                constexpr double tol = 1e-9;
                EXPECT_NEAR(expected.weightedDegree(u), G.weightedDegree(u), tol);
                EXPECT_NEAR(expected.weightedDegree(u, true), G.weightedDegree(u, true), tol);
                EXPECT_NEAR(expected.weightedDegreeIn(u), G.weightedDegreeIn(u), tol);
                EXPECT_NEAR(expected.weightedDegreeIn(u, true), G.weightedDegreeIn(u, true), tol);
            });
        }
    }
}

/** EDGE MODIFIERS **/

TEST_P(InducedSubgraphViewGTest, testHasEdge) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            expected.forNodes([&](node v) { EXPECT_EQ(expected.hasEdge(u, v), Gv.hasEdge(u, v)); });
        });
    }
}

/** GLOBAL PROPERTIES **/

TEST_P(InducedSubgraphViewGTest, testSelfLoopCountSimple) {
    this->Ghouse.addEdge(0, 0);
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        EXPECT_EQ(expected.numberOfSelfLoops(), Gv.numberOfSelfLoops());
    }
}

TEST_P(InducedSubgraphViewGTest, testIsWeighted) {
    InducedSubgraphView G = makeView(Ghouse, allNodes(Ghouse));
    ASSERT_EQ(isWeighted(), G.isWeighted());
}

TEST_P(InducedSubgraphViewGTest, testIsDirected) {
    InducedSubgraphView G = makeView(Ghouse, allNodes(Ghouse));
    ASSERT_EQ(isDirected(), G.isDirected());
}

TEST_P(InducedSubgraphViewGTest, testIsEmpty) {
    if (isCompact())
        GTEST_SKIP() << "mutates by specific global IDs; sparse-mode semantics";
    GraphW H = createGraphW(2);
    InducedSubgraphView G1(H, false);
    InducedSubgraphView G2(H, false);
    G2.addNodes(someNodes(H, 2));

    ASSERT_TRUE(G1.isEmpty());
    ASSERT_FALSE(G2.isEmpty());

    G1.addNode(0);
    G2.removeNodes({{0}});
    ASSERT_FALSE(G1.isEmpty());
    ASSERT_FALSE(G2.isEmpty());

    G1.removeNodes({{0}});
    G2.removeNodes({{1}});
    ASSERT_TRUE(G1.isEmpty());
    ASSERT_TRUE(G2.isEmpty());
}

TEST_P(InducedSubgraphViewGTest, testNumberOfNodes) {
    if (isCompact())
        GTEST_SKIP() << "mutates by specific global IDs; sparse-mode semantics";
    GraphW H = createGraphW(2);
    InducedSubgraphView G1(H, false);
    ASSERT_EQ(0u, G1.numberOfNodes());
    G1.addNode(0);
    ASSERT_EQ(1u, G1.numberOfNodes());
    G1.addNode(1);
    ASSERT_EQ(2u, G1.numberOfNodes());
    G1.removeNodes({{0}});
    ASSERT_EQ(1u, G1.numberOfNodes());
    G1.removeNodes({{1}});
    ASSERT_EQ(0u, G1.numberOfNodes());
}

TEST_P(InducedSubgraphViewGTest, testNumberOfEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        EXPECT_EQ(expected.numberOfEdges(), Gv.numberOfEdges());
    }
}

TEST_P(InducedSubgraphViewGTest, testNumberOfSelfLoops) {
    this->Ghouse.addEdge(0, 0);
    this->Ghouse.addEdge(1, 1);
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        EXPECT_EQ(expected.numberOfSelfLoops(), Gv.numberOfSelfLoops());
    }
}

TEST_P(InducedSubgraphViewGTest, testUpperNodeIdBound) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        EXPECT_EQ(expected.upperNodeIdBound(), Gv.upperNodeIdBound());
    }
}

/** EDGE ATTRIBUTES **/

TEST_P(InducedSubgraphViewGTest, testWeight) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            expected.forNodes([&](node v) { EXPECT_EQ(expected.weight(u, v), Gv.weight(u, v)); });
        });
    }
}

/** SUMS **/

TEST_P(InducedSubgraphViewGTest, testTotalEdgeWeight) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        EXPECT_DOUBLE_EQ(expected.totalEdgeWeight(), Gv.totalEdgeWeight());
    }
}

/** Collections **/

/** NODE ITERATORS **/

TEST_P(InducedSubgraphViewGTest, testForNodes) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);

        std::set<node> expectedSet, visited;
        expected.forNodes([&](node u) { expectedSet.insert(u); });
        G.forNodes([&](node v) { ASSERT_TRUE(visited.insert(v).second); });
        ASSERT_EQ(expectedSet, visited);
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelForNodes) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);

        std::vector<node> visited(G.upperNodeIdBound(), none);
        G.parallelForNodes([&](node u) { visited[u] = u; });

        std::set<node> expectedSet, seen;
        expected.forNodes([&](node u) { expectedSet.insert(u); });
        for (node u = 0; u < G.upperNodeIdBound(); ++u)
            if (visited[u] != none)
                seen.insert(visited[u]);
        ASSERT_EQ(expectedSet, seen);
    }
}

TEST_P(InducedSubgraphViewGTest, forNodesWhile) {
    GraphW H = createGraphW(100);
    InducedSubgraphView G = makeView(H, allNodes(H));
    count stopAfter = 10;
    count nodesSeen = 0;

    G.forNodesWhile([&]() { return nodesSeen < stopAfter; }, [&](node) { nodesSeen++; });

    ASSERT_EQ(stopAfter, nodesSeen);
}

TEST_P(InducedSubgraphViewGTest, testForNodesInRandomOrder) {
    count n = 1000;
    count samples = 100;
    double maxAbsoluteError = 0.005;
    GraphW H = createGraphW(n);
    InducedSubgraphView G = makeView(H, allNodes(H));

    node lastNode = n / 2;
    count greaterLastNode = 0;
    count smallerLastNode = 0;
    std::vector<count> visitCount(n, 0);

    for (count i = 0; i < samples; i++) {
        G.forNodesInRandomOrder([&](node v) {
            if (v > lastNode) {
                greaterLastNode++;
            } else {
                smallerLastNode++;
            }
            visitCount[v]++;
            lastNode = v;
        });
    }

    for (node v = 0; v < n; v++) {
        ASSERT_EQ(samples, visitCount[v]);
    }

    ASSERT_NEAR(0.5, (double)greaterLastNode / n / samples, maxAbsoluteError);
    ASSERT_NEAR(0.5, (double)smallerLastNode / n / samples, maxAbsoluteError);
}

TEST_P(InducedSubgraphViewGTest, testForNodePairs) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G = makeView(Ghouse, variant.nodes);
        const count k = G.numberOfNodes();

        count pairs = 0;
        std::set<std::pair<node, node>> seen;
        G.forNodePairs([&](node u, node v) {
            ++pairs;
            ASSERT_NE(u, v);
            ASSERT_TRUE(G.hasNode(u) && G.hasNode(v));
            node a = std::min(u, v), b = std::max(u, v);
            ASSERT_TRUE(seen.insert({a, b}).second);
        });
        EXPECT_EQ(k * (k - 1) / 2, pairs);
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelSumForNodes) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G = makeView(Ghouse, variant.nodes);
        double sum = G.parallelSumForNodes([](node v) { return 2 * v + 0.5; });

        double expected = 0;
        G.forNodes([&](node v) { expected += 2 * v + 0.5; });
        ASSERT_DOUBLE_EQ(expected, sum);
    }
}

/** EDGE ITERATORS **/

TEST_P(InducedSubgraphViewGTest, testForEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        auto norm = [&](node u, node v) {
            return Gv.isDirected() ? std::make_pair(u, v)
                                   : std::make_pair(std::min(u, v), std::max(u, v));
        };

        std::multiset<std::pair<node, node>> seen, ref;
        Gv.forEdges([&](node u, node v) {
            ASSERT_TRUE(Gv.hasEdge(u, v));
            seen.insert(norm(u, v));
        });
        expected.forEdges([&](node u, node v) { ref.insert(norm(u, v)); });
        EXPECT_EQ(ref, seen);
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        auto norm = [&](node u, node v) {
            return Gv.isDirected() ? std::make_pair(u, v)
                                   : std::make_pair(std::min(u, v), std::max(u, v));
        };

        std::multiset<std::pair<node, node>> seen, ref;
        edgeweight viewSum = 0, refSum = 0;
        Gv.forEdges([&](node u, node v, edgeweight ew) {
            ASSERT_TRUE(Gv.hasEdge(u, v));
            ASSERT_EQ(Gv.weight(u, v), ew);
            seen.insert(norm(u, v));
            viewSum += ew;
        });
        expected.forEdges([&](node u, node v, edgeweight ew) {
            ref.insert(norm(u, v));
            refSum += ew;
        });
        EXPECT_EQ(ref, seen);
        EXPECT_DOUBLE_EQ(refSum, viewSum);
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelForWeightedEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);

        double weightSum = 0.0;
        Gv.parallelForEdges([&](node, node, edgeweight ew) {
#pragma omp atomic
            weightSum += ew;
        });
        EXPECT_DOUBLE_EQ(expected.totalEdgeWeight(), weightSum);
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelForEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);

        count edges = 0;
        Gv.parallelForEdges([&](node, node) {
#pragma omp atomic
            ++edges;
        });
        EXPECT_EQ(expected.numberOfEdges(), edges);
    }
}

/** NEIGHBORHOOD ITERATORS **/

TEST_P(InducedSubgraphViewGTest, testForNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<node> seen, ref;
            Gv.forNeighborsOf(u, [&](node v) { seen.insert(v); });
            expected.forNeighborsOf(u, [&](node v) { ref.insert(v); });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<std::pair<node, edgeweight>> seen, ref;
            Gv.forNeighborsOf(u, [&](node v, edgeweight w) { seen.insert({v, w}); });
            expected.forNeighborsOf(u, [&](node v, edgeweight w) { ref.insert({v, w}); });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testForEdgesOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<node> seen, ref;
            Gv.forEdgesOf(u, [&](node v, node w) {
                EXPECT_EQ(u, v);
                seen.insert(w);
            });
            expected.forEdgesOf(u, [&](node v, node w) {
                EXPECT_EQ(u, v);
                ref.insert(w);
            });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedEdgesOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<std::pair<node, edgeweight>> seen, ref;
            Gv.forEdgesOf(u, [&](node v, node w, edgeweight ew) {
                EXPECT_EQ(u, v);
                seen.insert({w, ew});
            });
            expected.forEdgesOf(u, [&](node v, node w, edgeweight ew) {
                EXPECT_EQ(u, v);
                ref.insert({w, ew});
            });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelSumForWeightedEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);

        double sum = Gv.parallelSumForEdges([](node, node, edgeweight ew) { return 1.5 * ew; });
        EXPECT_DOUBLE_EQ(1.5 * expected.totalEdgeWeight(), sum);
    }
}

TEST_P(InducedSubgraphViewGTest, testForInNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<node> seen, ref;
            Gv.forInNeighborsOf(u, [&](node v) { seen.insert(v); });
            expected.forInNeighborsOf(u, [&](node v) { ref.insert(v); });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedInNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<std::pair<node, edgeweight>> seen, ref;
            Gv.forInNeighborsOf(u, [&](node v, edgeweight w) { seen.insert({v, w}); });
            expected.forInNeighborsOf(u, [&](node v, edgeweight w) { ref.insert({v, w}); });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testForInEdgesOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<node> seen, ref;
            Gv.forInEdgesOf(u, [&](node x, node v) {
                EXPECT_EQ(u, x);
                seen.insert(v);
            });
            expected.forInEdgesOf(u, [&](node x, node v) {
                EXPECT_EQ(u, x);
                ref.insert(v);
            });
            EXPECT_EQ(ref, seen);
        });
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedInEdgesOf) {
    this->Ghouse.addEdge(3, 3, 2.5);
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);
        expected.forNodes([&](node u) {
            std::multiset<std::pair<node, edgeweight>> seen, ref;
            Gv.forInEdgesOf(u, [&](node x, node v, edgeweight ew) {
                EXPECT_EQ(u, x);
                seen.insert({v, ew});
            });
            expected.forInEdgesOf(u, [&](node x, node v, edgeweight ew) {
                EXPECT_EQ(u, x);
                ref.insert({v, ew});
            });
            EXPECT_EQ(ref, seen);
        });
    }
}

/** COMPACT <-> GLOBAL BRIDGE (criterion 2) **/

TEST_P(InducedSubgraphViewGTest, testCompactToOriginalIdBridge) {
    if (!isCompact())
        GTEST_SKIP() << "criterion (2) is compact-mode only";

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW globalRef = GraphTools::subgraphFromNodes(Ghouse, variant.nodes.begin(),
                                                         variant.nodes.end(), false);

        EXPECT_EQ(globalRef.numberOfNodes(), Gv.upperNodeIdBound());
        std::set<node> localIds;
        Gv.forNodes([&](node u) { localIds.insert(u); });
        for (node i = 0; i < Gv.numberOfNodes(); ++i)
            EXPECT_TRUE(localIds.count(i)) << "missing local id " << i;

        Gv.forNodes([&](node u) { EXPECT_TRUE(globalRef.hasNode(Gv.toOriginalId(u))); });
        Gv.forEdges([&](node u, node v) {
            EXPECT_TRUE(globalRef.hasEdge(Gv.toOriginalId(u), Gv.toOriginalId(v)));
        });
        EXPECT_EQ(globalRef.numberOfEdges(), Gv.numberOfEdges());
    }
}

/** COMPATIBILITY WITH ALGORITHMS **/

TEST_P(InducedSubgraphViewGTest, testCoreDecomp) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv = makeView(Ghouse, variant.nodes);
        GraphW expected = reference(Ghouse, variant.nodes);

        auto coreDecompView = CoreDecomposition(Gv);
        auto coreDecomp = CoreDecomposition(expected);
        coreDecompView.run();
        coreDecomp.run();

        expected.forNodes(
            [&](node u) { EXPECT_DOUBLE_EQ(coreDecomp.score(u), coreDecompView.score(u)); });
    }
}

/*
 * A view of a view is not expressible: the constructors take the concrete graphs, so neither a
 * view nor a handle that might hold one can be a base. This is a compile-time property, and these
 * assertions are what keep it one.
 */
static_assert(std::is_constructible_v<InducedSubgraphView, const GraphW &, bool>);
static_assert(std::is_constructible_v<InducedSubgraphView, const GraphR &, bool>);
static_assert(!std::is_constructible_v<InducedSubgraphView, const InducedSubgraphView &, bool>,
              "a view must not be viewable");
static_assert(!std::is_constructible_v<InducedSubgraphView, const Graph &, bool>,
              "a handle may hold a view, so it must not be a base either");

} /* namespace NetworKit */
