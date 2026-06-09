/*
 * RangeMinimumQuery.hpp
 *
 *  Created on: 06.05.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_AUXILIARY_RANGE_MINIMUM_QUERY_HPP_
#define NETWORKIT_AUXILIARY_RANGE_MINIMUM_QUERY_HPP_

#include <vector>

#include <networkit/Globals.hpp>

namespace Aux {

using index = NetworKit::index;
using count = NetworKit::count;

/**
 * @ingroup Aux
 * Range minimum query for integer keys.
 * Uses the sparse stable structure, following
 *
 * Bender, M.A., Farach-Colton, M. (2000).
 * The LCA Problem Revisited.
 * Theoretical Informatics. LATIN 2000.
 * https://doi.org/10.1007/10719839_9
 */
class RangeMinimumQuery {
private:
    const std::vector<int64_t> *data;
    std::vector<index> st; // st[j][i] index of minimum value in [i, i + 2^j)
    index n, logn;         // shape

public:
    /**
     * Builds RMQ from the input data
     * @param[in] data Input data
     */
    RangeMinimumQuery(const std::vector<int64_t> &data);
    RangeMinimumQuery() = default;

    /**
     * Finds the index of the minimum value in [leftRange, rightRange)
     * @param[in] leftRange Inclusive left range
     * @param[in] rightRange exclusive right range
     *
     * @param[out]    index of the minimum value in [leftRange, rightRange)
     */
    index Query(index leftRange, index rightRange);

    RangeMinimumQuery(RangeMinimumQuery &&) noexcept = default;
    RangeMinimumQuery &operator=(RangeMinimumQuery &&) noexcept = default;
};

} /* namespace Aux */
#endif // NETWORKIT_AUXILIARY_RANGE_MINIMUM_QUERY_HPP_
