/*
 * UnionFindSubset.hpp
 *
 *  Created on: 06.10.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 *
 */

#include <algorithm>
#include <utility>
#include <networkit/structures/UnionFindSubset.hpp>

namespace NetworKit {

UnionFindSubset::UnionFindSubset(index max_element)
    : uf(max_element), size(max_element, 1), next(max_element) {
    allToSingletons();
}

std::vector<node> UnionFindSubset::subset(index u) {
    std::vector<node> out{u};
    for (index it = next[u]; it != u; it = next[it])
        out.push_back(it);
    return out;
}

count UnionFindSubset::subsetSize(index u) {
    return size[u];
}

void UnionFindSubset::allToSingletons() {
    uf.allToSingletons();
    for (index u = 0; u < size.size(); ++u) {
        size[u] = 1;
        next[u] = u;
    }
}

index UnionFindSubset::find(index u) {
    return uf.find(u);
}

void UnionFindSubset::merge(index u, index v) {
    index ur = uf.find(u), vr = uf.find(v);
    if (ur == vr)
        return;

    uf.merge(u, v);
    index root = uf.find(u);
    size[root] += size[(ur == root) ? vr : ur];
    std::swap(next[ur], next[vr]);
}

Partition UnionFindSubset::toPartition() {
    return uf.toPartition();
}

} // namespace NetworKit
