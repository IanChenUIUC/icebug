/*
 * SteinerKCore.hpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_STEINER_KCORE_HPP_
#define NETWORKIT_SCD_STEINER_KCORE_HPP_

#include <networkit/scd/SelectiveCommunityDetector.hpp>

namespace NetworKit {

/**
 * The steiner community expansion algorithm optimizing k-core.
 *
 */
class SteinerKCore : public SelectiveCommunityDetector {
    const std::vector<count> &coreness;

public:
    /**
     * Constructs the Steiner KCore algorithm.
     *
     * @param[in] G The graph to detect communities on
     */
    SteinerKCore(const Graph &g, const std::vector<count> &coreness);

    /**
     * Expands a set of seed nodes into a community.
     *
     * @param[in] s The seed nodes
     * @return A community of the seed nodes
     */
    std::set<node> expandOneCommunity(const std::set<node> &s) override;

    /**
     * Detect a community for the given seed nodes.
     *
     * @param seed The seeds to find the community for.
     * @return The found community as set of node.
     */
    std::vector<std::set<node>> run(const std::vector<std::set<node>> &s);

    using SelectiveCommunityDetector::expandOneCommunity;
};

} // namespace NetworKit

#endif // NETWORKIT_SCD_STEINER_KCORE_HPP_
