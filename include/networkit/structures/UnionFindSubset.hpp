/*
 * UnionFindSubset.hpp
 *
 *  Created on: 06.10.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 *
 */

#ifndef NETWORKIT_STRUCTURES_UNION_FIND_SUBSET_HPP_
#define NETWORKIT_STRUCTURES_UNION_FIND_SUBSET_HPP_

#include <networkit/structures/UnionFind.hpp>

namespace NetworKit {

/**
 * @ingroup structures
 * UnionFind that additionally supports subset queries, i.e.
 * finding all other elements in the same component.
 *
 * This implementation has constant time overhead on existing find/merge,
 * via a circular linked list.
 */
class UnionFindSubset {
    UnionFind uf;
    std::vector<index> next;
    std::vector<count> size;

public:
    /**
     * Create a new set representation with not more than @p max_element elements.
     * Initially every element is in its own set.
     * @param max_element maximum number of elements
     */
    UnionFindSubset(index max_element);

    /**
     * Finds all elements in the same subset as u.
     * @param u element
     */
    std::vector<node> subset(index u);

    /**
     * Finds the number of elements in the same component as u.
     * @param u element
     */
    count subsetSize(index u);

    /**
     * Assigns every element to a singleton set.
     * Set id is equal to element id.
     */
    void allToSingletons();

    /**
     * Find the representative to element @u
     * @param u element
     * @return representative of set containing @u
     */
    index find(index u);

    /**
     *  Merge the two sets contain @u and @v
     *  @param u element u
     *  @param v element v
     */
    void merge(index u, index v);

    /**
     * Convert the Union Find data structure to a Partition
     * @return Partition equivalent to the union find data structure
     * */
    Partition toPartition();
};
} /* namespace NetworKit */

#endif // NETWORKIT_STRUCTURES_UNION_FIND_SUBSET_HPP_
