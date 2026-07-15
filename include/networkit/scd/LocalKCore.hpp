/*
 * LocalKCore.hpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_LOCAL_K_CORE_HPP_
#define NETWORKIT_SCD_LOCAL_K_CORE_HPP_

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
     * @param[in] logFile
     */
    LocalKCore(const Graph &g, const std::string &logFile = "");

    /**
     * Expands a set of seed nodes into a community.
     *
     * @param[in] s The seed nodes
     * @return A community of the seed nodes
     */
    std::set<node> expandOneCommunity(const std::set<node> &s) override;

    // std::set<node> expandOneCommunity(node s) override;

    using SelectiveCommunityDetector::expandOneCommunity;

private:
    std::string logFile;
};

} // namespace NetworKit

#endif // NETWORKIT_SCD_LOCAL_K_CORE_HPP_
