/*
 * LocalKCore.hpp
 *
 *  Created on: 06.10.26
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_LOCAL_K_CORE_HPP_
#define NETWORKIT_SCD_LOCAL_K_CORE_HPP_

#include <vector>

#include <networkit/Globals.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>

namespace NetworKit {

/**
 * The state of the search at the moment the lower bound was recomputed.
 */
struct TriggerStats {
    count inducedVolume; // volume of the candidate subgraph
    count outVolume;     // volume of the candidates in the base graph
    count potential;     // neighborhoods scanned so far, weighted by degree
    count numExplored;   // nodes that have ever been a candidate
    count numCandidates; // nodes that are currently a candidate
    count numRemoved;    // nodes proven to be outside the community
    count lower;         // lower bound on the coreness of the query
    count upper;         // upper bound on the coreness of the query
};

/**
 * The local community expansion algorithm optimizing k-core.
 *
 */
class LocalKCore : public SelectiveCommunityDetector {
public:
    /**
     * Constructs the Local KCore algorithm.
     *
     * @param[in] g The graph to detect communities on. Must be undirected, unweighted,
     *              free of self loops, and not a view.
     * @param[in] maximal Whether to return the largest community.
     * @param[in] tightenUpper Whether to maintain h-index upper bounds on the coreness. When
     *              false, pruning falls back to the degree, which is always a valid bound.
     */
    LocalKCore(const Graph &g, bool maximal = true, bool tightenUpper = false);

    /**
     * Expands a set of seed nodes into a community.
     *
     * @param[in] s The seed nodes
     * @return A community of the seed nodes
     */
    std::set<node> expandOneCommunity(const std::set<node> &s) override;

    using SelectiveCommunityDetector::expandOneCommunity;

    /**
     * The trace of the last call to expandOneCommunity(), one entry per recomputation of
     * the lower bound.
     */
    const std::vector<TriggerStats> &getTriggerStats() const { return stats; }

    /**
     * Checks the invariants of the search after every iteration, throwing std::logic_error
     * on the first violation. Computes the answer on the whole graph up front, so this is
     * only usable on small graphs.
     */
    void setValidate(bool v) { validate = v; }

private:
    bool maximal;
    bool tightenUpper;
    bool validate = false;
    std::vector<TriggerStats> stats;
};

} // namespace NetworKit

#endif // NETWORKIT_SCD_LOCAL_K_CORE_HPP_
