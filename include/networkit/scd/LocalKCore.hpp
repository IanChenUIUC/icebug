/*
 * LocalKCore.hpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_LOCAL_KCORE_HPP_
#define NETWORKIT_SCD_LOCAL_KCORE_HPP_

#include <networkit/scd/SelectiveCommunityDetector.hpp>

namespace NetworKit {

/**
 * The local community expansion algorithm optimizing k-core.
 *
 */
class LocalKCore : public SelectiveCommunityDetector {
public:
    /**
     * Constructs the Local KCore algorithm.
     *
     * @param[in] G The graph to detect communities on
     */
    LocalKCore(const Graph &g);

    /**
     * Expands a set of seed nodes into a community.
     *
     * @param[in] s The seed nodes
     * @return A community of the seed nodes
     */
    std::set<node> expandOneCommunity(const std::set<node> &s) override;

    // std::set<node> expandOneCommunity(node s) override;

    using SelectiveCommunityDetector::expandOneCommunity;
};

} // namespace NetworKit

#endif // NETWORKIT_SCD_LOCAL_KCORE_HPP_
