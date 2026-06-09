/*
 * RangeMinimumQuery.cpp
 *
 *  Created on: 06.05.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <networkit/Globals.hpp>
#include <networkit/auxiliary/RangeMinimumQuery.hpp>

namespace Aux {

RangeMinimumQuery::RangeMinimumQuery(const std::vector<int64_t> &data)
    : data(&data), n(data.size()), logn(0) {
    if (n == 0)
        return;

    logn = std::__lg(n) + 1;
    st.resize(n * logn);
    auto idx = [&](size_t i, size_t j) { return j * n + i; };

#pragma omp parallel for
    for (index i = 0; i < n; ++i)
        st[idx(i, 0)] = i;

    for (size_t j = 1; j < logn; ++j) {
        size_t range_len = 1ULL << (j - 1);
        index bound = n - (1ULL << j) + 1;

#pragma omp parallel for
        for (index i = 0; i < bound; ++i) {
            size_t left_idx = st[idx(i, j - 1)];
            size_t right_idx = st[idx(i + range_len, j - 1)];

            if (data[left_idx] <= data[right_idx]) {
                st[idx(i, j)] = left_idx;
            } else {
                st[idx(i, j)] = right_idx;
            }
        }
    }
}

index RangeMinimumQuery::Query(index leftRange, index rightRange) {
    if (leftRange >= rightRange)
        return NetworKit::none;

    auto idx = [&](size_t i, size_t j) { return j * n + i; };
    index j = std::__lg(rightRange - leftRange);
    index range_len = 1ULL << j;

    index left_idx = st[idx(leftRange, j)];
    index right_idx = st[idx(rightRange - range_len, j)];

    if ((*data)[left_idx] <= (*data)[right_idx])
        return left_idx;
    return right_idx;
}

} // namespace Aux
