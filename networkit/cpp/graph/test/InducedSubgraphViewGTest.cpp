/*
 * InducedSubgraphViewGTest.cpp
 *
 *  Created on: 06.26.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>

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

/** EDGE MODIFIERS **/

TEST_P(InducedSubgraphViewGTest, testHasEdge) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u = 0; u < Ghouse.upperNodeIdBound(); ++u)
            for (node v = 0; v < Ghouse.upperNodeIdBound(); ++v)
                EXPECT_EQ(expected.hasEdge(u, v), Gv.hasEdge(u, v));
    }
}

/** GLOBAL PROPERTIES **/

TEST_P(InducedSubgraphViewGTest, testSelfLoopCountSimple) {
    this->Ghouse.addEdge(0, 0);
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        EXPECT_EQ(expected.numberOfSelfLoops(), Gv.numberOfSelfLoops());
    }
}

TEST_P(InducedSubgraphViewGTest, testIsWeighted) {
    InducedSubgraphView G(Ghouse, allNodes(Ghouse));
    ASSERT_EQ(isWeighted(), G.isWeighted());
}

TEST_P(InducedSubgraphViewGTest, testIsDirected) {
    InducedSubgraphView G(Ghouse, allNodes(Ghouse));
    ASSERT_EQ(isDirected(), G.isDirected());
}

TEST_P(InducedSubgraphViewGTest, testIsEmpty) {
    GraphW H = createGraphW(2);
    InducedSubgraphView G1(H, std::set<node>{});
    InducedSubgraphView G2(H, someNodes(H, 2));

    ASSERT_TRUE(G1.isEmpty());
    ASSERT_FALSE(G2.isEmpty());

    G1.addNode(0);
    G2.removeNodes({0});
    ASSERT_FALSE(G1.isEmpty());
    ASSERT_FALSE(G2.isEmpty());

    G1.removeNodes({0});
    G2.removeNodes({1});
    ASSERT_TRUE(G1.isEmpty());
    ASSERT_TRUE(G2.isEmpty());
}

TEST_P(InducedSubgraphViewGTest, testNumberOfNodes) {
    GraphW H = createGraphW(2);
    InducedSubgraphView G1(H, std::set<node>{});
    ASSERT_EQ(0u, G1.numberOfNodes());
    G1.addNode(0);
    ASSERT_EQ(1u, G1.numberOfNodes());
    G1.addNode(1);
    ASSERT_EQ(2u, G1.numberOfNodes());
    G1.removeNodes({0});
    ASSERT_EQ(1u, G1.numberOfNodes());
    G1.removeNodes({1});
    ASSERT_EQ(0u, G1.numberOfNodes());
}

TEST_P(InducedSubgraphViewGTest, testNumberOfEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        EXPECT_EQ(expected.numberOfEdges(), Gv.numberOfEdges());
    }
}

TEST_P(InducedSubgraphViewGTest, testNumberOfSelfLoops) {
    this->Ghouse.addEdge(0, 0);
    this->Ghouse.addEdge(1, 1);
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        EXPECT_EQ(expected.numberOfSelfLoops(), Gv.numberOfSelfLoops());
    }
}

TEST_P(InducedSubgraphViewGTest, testUpperNodeIdBound) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        EXPECT_EQ(Ghouse.upperNodeIdBound(), Gv.upperNodeIdBound());
    }
}

/** EDGE ATTRIBUTES **/

TEST_P(InducedSubgraphViewGTest, testWeight) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u = 0; u < Ghouse.upperNodeIdBound(); ++u)
            for (node v = 0; v < Ghouse.upperNodeIdBound(); ++v)
                EXPECT_EQ(expected.weight(u, v), Gv.weight(u, v));
    }
}

/** SUMS **/

TEST_P(InducedSubgraphViewGTest, testTotalEdgeWeight) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        EXPECT_DOUBLE_EQ(expected.totalEdgeWeight(), Gv.totalEdgeWeight());
    }
}

/** Collections **/

TEST_P(InducedSubgraphViewGTest, testNodeIterator) {
    Aux::Random::setSeed(42, false);

    auto testForward = [](const InducedSubgraphView &G) {
        auto preIter = G.nodeRange().begin();
        auto postIter = G.nodeRange().begin();

        G.forNodes([&](const node u) {
            ASSERT_EQ(*preIter, u);
            ASSERT_EQ(*postIter, u);
            ++preIter;
            postIter++;
        });

        ASSERT_EQ(preIter, G.nodeRange().end());
        ASSERT_EQ(postIter, G.nodeRange().end());

        InducedSubgraphView G1(G);
        for (const auto u : InducedSubgraphView::NodeRange(G)) {
            ASSERT_TRUE(G1.hasNode(u));
            G1.removeNodes({u});
        }
        ASSERT_EQ(G1.numberOfNodes(), 0u);
    };

    auto testBackward = [](const InducedSubgraphView &G) {
        const std::vector<node> nodes(InducedSubgraphView::NodeRange(G).begin(),
                                      InducedSubgraphView::NodeRange(G).end());
        std::vector<node> v;
        G.forNodes([&](node u) { v.push_back(u); });

        ASSERT_EQ(std::unordered_set<node>(nodes.begin(), nodes.end()).size(), nodes.size());
        ASSERT_EQ(nodes.size(), G.numberOfNodes());

        auto preIter = G.nodeRange().begin();
        auto postIter = G.nodeRange().begin();
        for (count i = 0; i < G.numberOfNodes(); ++i) {
            ++preIter;
            postIter++;
        }

        ASSERT_EQ(preIter, G.nodeRange().end());
        ASSERT_EQ(postIter, G.nodeRange().end());
        auto vecIter = nodes.rbegin();
        while (vecIter != nodes.rend()) {
            ASSERT_EQ(*vecIter, *(--preIter));
            if (postIter != G.nodeRange().end()) {
                ASSERT_NE(*vecIter, *(postIter--));
            } else {
                postIter--;
            }
            ASSERT_EQ(*vecIter, *postIter);
            ++vecIter;
        }

        ASSERT_EQ(preIter, G.nodeRange().begin());
        ASSERT_EQ(postIter, G.nodeRange().begin());
    };

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G(Ghouse, variant.nodes);
        testForward(G);
        testBackward(G);
    }
}

TEST_P(InducedSubgraphViewGTest, testEdgeIterator) {
    auto testForward = [&](const InducedSubgraphView &G) {
        auto preIter = G.edgeRange().begin();
        auto postIter = G.edgeRange().begin();

        count edges = 0;
        G.forEdges([&](node, node) {
            ASSERT_EQ(preIter, postIter);
            const auto edge = *preIter;
            ASSERT_TRUE(G.hasEdge(edge.u, edge.v));
            ++preIter;
            postIter++;
            ++edges;
        });

        ASSERT_EQ(preIter, G.edgeRange().end());
        ASSERT_EQ(postIter, G.edgeRange().end());

        edges = 0;
        for (const auto edge : InducedSubgraphView::EdgeRange(G)) {
            ASSERT_TRUE(G.hasEdge(edge.u, edge.v));
            ++edges;
        }
        ASSERT_EQ(edges, G.numberOfEdges());
    };

    auto testForwardWeighted = [&](const InducedSubgraphView &G) {
        auto preIter = G.edgeWeightRange().begin();
        auto postIter = preIter;

        G.forEdges([&](node, node) {
            ASSERT_EQ(preIter, postIter);
            const auto edge = *preIter;
            ASSERT_TRUE(G.hasEdge(edge.u, edge.v));
            ASSERT_DOUBLE_EQ(G.weight(edge.u, edge.v), edge.weight);
            ++preIter;
            postIter++;
        });

        ASSERT_EQ(preIter, G.edgeWeightRange().end());
        ASSERT_EQ(postIter, G.edgeWeightRange().end());

        count edges = 0;
        for (const auto edge : InducedSubgraphView::EdgeWeightRange(G)) {
            ASSERT_TRUE(G.hasEdge(edge.u, edge.v));
            ASSERT_DOUBLE_EQ(G.weight(edge.u, edge.v), edge.weight);
            ++edges;
        }
        ASSERT_EQ(edges, G.numberOfEdges());
    };

    auto testBackward = [&](const InducedSubgraphView &G) {
        auto preIter = G.edgeRange().begin();
        auto postIter = preIter;
        G.forEdges([&](node, node) {
            ++preIter;
            postIter++;
        });

        ASSERT_EQ(preIter, G.edgeRange().end());
        ASSERT_EQ(postIter, G.edgeRange().end());

        G.forEdges([&](node, node) {
            --preIter;
            postIter--;
            ASSERT_EQ(preIter, postIter);
            const auto edge = *preIter;
            ASSERT_TRUE(G.hasEdge(edge.u, edge.v));
        });

        ASSERT_EQ(preIter, G.edgeRange().begin());
        ASSERT_EQ(postIter, G.edgeRange().begin());
    };

    auto testBackwardWeighted = [&](const InducedSubgraphView &G) {
        auto preIter = G.edgeWeightRange().begin();
        auto postIter = preIter;
        G.forEdges([&](node, node) {
            ++preIter;
            postIter++;
        });

        G.forEdges([&](node, node) {
            --preIter;
            postIter--;
            ASSERT_EQ(preIter, postIter);
            const auto edge = *preIter;
            ASSERT_TRUE(G.hasEdge(edge.u, edge.v));
            ASSERT_DOUBLE_EQ(G.weight(edge.u, edge.v), edge.weight);
        });
    };

    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G(Ghouse, variant.nodes);
        testForward(G);
        testBackward(G);
        testForwardWeighted(G);
        testBackwardWeighted(G);
    }
}

TEST_P(InducedSubgraphViewGTest, testNeighborsIterators) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
            auto range = G.neighborRange(u);
            auto iter = range.begin();
            G.forNeighborsOf(u, [&](node v) {
                ASSERT_TRUE(*iter == v);
                ++iter;
            });
            ASSERT_TRUE(iter == range.end());

            if (G.isWeighted()) {
                auto range = G.weightNeighborRange(u);
                auto iterW = range.begin();
                G.forNeighborsOf(u, [&](node v, edgeweight w) {
                    ASSERT_TRUE((*iterW).first == v);
                    ASSERT_TRUE((*iterW).second == w);
                    ++iterW;
                });
                ASSERT_TRUE(iterW == range.end());
            }

            if (G.isDirected()) {
                auto range = G.inNeighborRange(u);
                auto inIter = range.begin();
                G.forInNeighborsOf(u, [&](node v) {
                    ASSERT_TRUE(*inIter == v);
                    ++inIter;
                });
                ASSERT_TRUE(inIter == range.end());

                if (G.isWeighted()) {
                    auto range = G.weightInNeighborRange(u);
                    auto iterWin = range.begin();
                    G.forInNeighborsOf(u, [&](node v, edgeweight w) {
                        ASSERT_TRUE((*iterWin).first == v);
                        ASSERT_TRUE((*iterWin).second == w);
                        ++iterWin;
                    });
                    ASSERT_TRUE(iterWin == range.end());
                }
            }
        }
    }
}

/** NODE ITERATORS **/

TEST_P(InducedSubgraphViewGTest, testForNodes) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G(Ghouse, variant.nodes);
        std::set<node> visited;
        G.forNodes([&](node v) { ASSERT_TRUE(visited.insert(v).second); });
        ASSERT_EQ(variant.nodes, visited);
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelForNodes) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G(Ghouse, variant.nodes);
        std::vector<node> visited(G.upperNodeIdBound(), none);
        G.parallelForNodes([&](node u) { visited[u] = u; });

        std::set<node> seen;
        for (node u = 0; u < G.upperNodeIdBound(); ++u)
            if (visited[u] != none)
                seen.insert(visited[u]);
        ASSERT_EQ(variant.nodes, seen);
    }
}

TEST_P(InducedSubgraphViewGTest, forNodesWhile) {
    GraphW H = createGraphW(100);
    InducedSubgraphView G(H, allNodes(H));
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
    InducedSubgraphView G(H, allNodes(H));

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
        InducedSubgraphView G(Ghouse, variant.nodes);
        const count k = variant.nodes.size();

        count pairs = 0;
        std::set<std::pair<node, node>> seen;
        G.forNodePairs([&](node u, node v) {
            ++pairs;
            ASSERT_NE(u, v);
            ASSERT_TRUE(variant.nodes.count(u) && variant.nodes.count(v));
            node a = std::min(u, v), b = std::max(u, v);
            ASSERT_TRUE(seen.insert({a, b}).second);
        });
        EXPECT_EQ(k * (k - 1) / 2, pairs);
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelSumForNodes) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView G(Ghouse, variant.nodes);
        double sum = G.parallelSumForNodes([](node v) { return 2 * v + 0.5; });

        double expected = 0;
        for (node v : variant.nodes)
            expected += 2 * v + 0.5;
        ASSERT_DOUBLE_EQ(expected, sum);
    }
}

/** EDGE ITERATORS **/

TEST_P(InducedSubgraphViewGTest, testForEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
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
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
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
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);

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
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);

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
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
            std::multiset<node> seen, ref;
            Gv.forNeighborsOf(u, [&](node v) { seen.insert(v); });
            expected.forNeighborsOf(u, [&](node v) { ref.insert(v); });
            EXPECT_EQ(ref, seen);
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
            std::multiset<std::pair<node, edgeweight>> seen, ref;
            Gv.forNeighborsOf(u, [&](node v, edgeweight w) { seen.insert({v, w}); });
            expected.forNeighborsOf(u, [&](node v, edgeweight w) { ref.insert({v, w}); });
            EXPECT_EQ(ref, seen);
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testForEdgesOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
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
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedEdgesOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
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
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testParallelSumForWeightedEdges) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);

        double sum = Gv.parallelSumForEdges([](node, node, edgeweight ew) { return 1.5 * ew; });
        EXPECT_DOUBLE_EQ(1.5 * expected.totalEdgeWeight(), sum);
    }
}

TEST_P(InducedSubgraphViewGTest, testForInNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
            std::multiset<node> seen, ref;
            Gv.forInNeighborsOf(u, [&](node v) { seen.insert(v); });
            expected.forInNeighborsOf(u, [&](node v) { ref.insert(v); });
            EXPECT_EQ(ref, seen);
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedInNeighborsOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
            std::multiset<std::pair<node, edgeweight>> seen, ref;
            Gv.forInNeighborsOf(u, [&](node v, edgeweight w) { seen.insert({v, w}); });
            expected.forInNeighborsOf(u, [&](node v, edgeweight w) { ref.insert({v, w}); });
            EXPECT_EQ(ref, seen);
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testForInEdgesOf) {
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
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
        }
    }
}

TEST_P(InducedSubgraphViewGTest, testForWeightedInEdgesOf) {
    this->Ghouse.addEdge(3, 3, 2.5);
    for (const auto &variant : subsetVariants(Ghouse)) {
        SCOPED_TRACE(variant.label);
        InducedSubgraphView Gv(Ghouse, variant.nodes);
        GraphW expected = inducedSubgraph(Ghouse, variant.nodes);
        for (node u : variant.nodes) {
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
