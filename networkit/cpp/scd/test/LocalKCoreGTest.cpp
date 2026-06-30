/*
 * LocalKCoreGTest.cpp
 *
 *  Created on: 06.30.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <numeric>
#include <vector>
#include <gtest/gtest.h>
#include <networkit/graph/GraphW.hpp>
#include <networkit/scd/LocalKCore.hpp>

namespace NetworKit {

class LocalKCoreGTest : public ::testing::Test {
protected:
    GraphW getCliqueChain(const std::vector<count> &cliqueSizes) {
        count totalNodes = 0;
        for (count size : cliqueSizes)
            totalNodes += size;

        GraphW g(totalNodes, false, false);
        count start = 0;
        for (count size : cliqueSizes) {
            for (count u = start; u < start + size; ++u)
                for (count v = u + 1; v < start + size; ++v)
                    g.addEdge(u, v);
            start += size;
        }

        start = 0;
        for (count i = 0; i < cliqueSizes.size() - 1; ++i) {
            start += cliqueSizes[i];
            g.addEdge(start - 1, start);
        }

        return g;
    }

    std::set<node> gtSearch(const std::vector<count> &cliques, const std::set<node> &query) {
        std::vector<count> boundaries;
        count currentSum = 0;
        for (count size : cliques) {
            currentSum += size;
            boundaries.push_back(currentSum);
        }

        count cMin = cliques.size();
        count cMax = 0;

        for (node q : query) {
            count cIdx = 0;
            for (count i = 0; i < boundaries.size(); ++i) {
                if (q < boundaries[i]) {
                    cIdx = i;
                    break;
                }
            }
            cMin = std::min(cMin, cIdx);
            cMax = std::max(cMax, cIdx);
        }

        count bottleneckSize = cliques[cMin];
        for (count i = cMin; i <= cMax; ++i)
            bottleneckSize = std::min(bottleneckSize, cliques[i]);

        count left = cMin;
        while (left > 0 && cliques[left - 1] >= bottleneckSize)
            left--;

        count right = cMax;
        while (right < cliques.size() - 1 && cliques[right + 1] >= bottleneckSize)
            right++;

        count vBeg = (left > 0) ? boundaries[left - 1] : 0;
        count vEnd = boundaries[right];

        std::set<node> expectedCommunity;
        for (count v = vBeg; v < vEnd; ++v) {
            expectedCommunity.insert(static_cast<node>(v));
        }

        return expectedCommunity;
    }

    void verifySingletons(LocalKCore &local, const std::vector<count> &cliques) {
        count n = std::accumulate(cliques.begin(), cliques.end(), count(0));
        for (node v = 0; v < n; ++v) {
            std::set<node> query = {v};
            std::set<node> et = local.expandOneCommunity(query);
            std::set<node> gt = gtSearch(cliques, query);

            EXPECT_EQ(et, gt) << "Failed on singleton query node: " << v;
        }
    }

    void verifyPairs(LocalKCore &local, const std::vector<count> &cliques) {
        count n = std::accumulate(cliques.begin(), cliques.end(), count(0));
        for (node u = 0; u < n; ++u) {
            for (node v = u + 1; v < n; ++v) {
                std::set<node> query = {u, v};
                std::set<node> et = local.expandOneCommunity(query);
                std::set<node> gt = gtSearch(cliques, query);

                EXPECT_EQ(et, gt) << "Failed on pair nodes: {" << u << ", " << v << "}";
            }
        }
    }
};

TEST_F(LocalKCoreGTest, SeparatedCores_Singleton) {
    std::vector<count> cliques{4, 3, 4};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifySingletons(local, cliques);
}

TEST_F(LocalKCoreGTest, SeparatedCores_Pairs) {
    std::vector<count> cliques{4, 3, 4};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifyPairs(local, cliques);
}

TEST_F(LocalKCoreGTest, BridgedCore_Singleton) {
    std::vector<count> cliques{3, 4, 3};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifySingletons(local, cliques);
}

TEST_F(LocalKCoreGTest, BridgedCore_Pairs) {
    std::vector<count> cliques{3, 4, 3};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifyPairs(local, cliques);
}

TEST_F(LocalKCoreGTest, Medium_Singleton) {
    std::vector<count> cliques{3, 5, 4, 3, 6, 5, 4, 6, 3};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifySingletons(local, cliques);
}

TEST_F(LocalKCoreGTest, Medium_Pairs) {
    std::vector<count> cliques{3, 5, 4, 3, 6, 5, 4, 6, 3};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifyPairs(local, cliques);
}

TEST_F(LocalKCoreGTest, Large_Singleton) {
    std::vector<count> cliques{3, 4, 5, 6, 5, 4, 3, 5, 7, 9, 11, 9, 7, 5, 3, 4, 5, 6, 4, 3};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifySingletons(local, cliques);
}

TEST_F(LocalKCoreGTest, LargePairs) {
    std::vector<count> cliques{3, 4, 5, 6, 5, 4, 3, 5, 7, 9, 11, 9, 7, 5, 3, 4, 5, 6, 4, 3};
    GraphW g = getCliqueChain(cliques);
    LocalKCore local(g);
    verifyPairs(local, cliques);
}

} /* namespace NetworKit */
