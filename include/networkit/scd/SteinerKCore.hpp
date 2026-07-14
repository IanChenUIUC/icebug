/*
 * SteinerKCore.hpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_STEINER_K_CORE_HPP_
#define NETWORKIT_SCD_STEINER_K_CORE_HPP_

#include <networkit/scd/SelectiveCommunityDetector.hpp>

namespace NetworKit {

/**
 * The steiner community expansion algorithm optimizing k-core.
 *
 */
class SteinerKCore : public SelectiveCommunityDetector {
    std::vector<count> ownedCoreness; // for the convenience constructor
    std::span<const count> coreness;

public:
    /**
     * Constructs the Steiner KCore algorithm.
     *
     * @param[in] G The graph to detect communities on
     * @param[in] coreness The pre-computed core decomposition for
          the graph. coreness must be kept alive as long as this is.
     */
    SteinerKCore(const Graph &g, std::span<const count> coreness);

    SteinerKCore(const Graph &g);

    /**
     * Find the coreness of the query set.
     *
     * @param[in] s The seed nodes
     * @return Minimum degree of the output of expandOneCommunity
     */
    count queryCoreness(const std::set<node> &s);

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

#endif // NETWORKIT_SCD_STEINER_K_CORE_HPP_
