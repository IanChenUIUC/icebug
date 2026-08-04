/*
 * NeighborIterators.hpp
 *
 *  Created on: 16.09.2024
 */

#ifndef NETWORKIT_GRAPH_NEIGHBOR_ITERATORS_HPP_
#define NETWORKIT_GRAPH_NEIGHBOR_ITERATORS_HPP_

#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

#include <networkit/Globals.hpp>

namespace NetworKit {

/**
 * Class to iterate over the in/out neighbors of a node.
 *
 * Templated on the iterator rather than the container so that a neighborhood may be a slice of
 * a larger array, not only a whole container.
 */
template <typename elementIterType>
class NeighborIteratorBase {

    elementIterType elementIter;

public:
    // The value type of the neighbors (i.e. nodes). Returned by
    // operator*().
    using value_type = typename std::iterator_traits<elementIterType>::value_type;

    // Reference to the value_type, required by STL.
    using reference = value_type &;

    // Pointer to the value_type, required by STL.
    using pointer = value_type *;

    // STL iterator category.
    using iterator_category = std::forward_iterator_tag;

    // Signed integer type of the result of subtracting two pointers,
    // required by STL.
    using difference_type = ptrdiff_t;

    // Own type.
    using self = NeighborIteratorBase;

    NeighborIteratorBase(elementIterType elementIter) : elementIter(elementIter) {}

    /**
     * @brief WARNING: This contructor is required for Python and should not be used as the
     * iterator is not initialized.
     */
    NeighborIteratorBase() : elementIter{} {}

    NeighborIteratorBase &operator++() {
        ++elementIter;
        return *this;
    }

    NeighborIteratorBase operator++(int) {
        const auto tmp = *this;
        ++elementIter;
        return tmp;
    }

    NeighborIteratorBase operator--() {
        const auto tmp = *this;
        --elementIter;
        return tmp;
    }

    NeighborIteratorBase operator--(int) {
        --elementIter;
        return *this;
    }

    bool operator==(const NeighborIteratorBase &rhs) const {
        return elementIter == rhs.elementIter;
    }

    bool operator!=(const NeighborIteratorBase &rhs) const {
        return !(elementIter == rhs.elementIter);
    }

    value_type operator*() const { return *elementIter; }
};

/**
 * Class to iterate over the in/out neighbors of a node, including the weights given by a
 * parallel weight sequence.
 */
template <typename elementIterType, typename weightIterType>
class NeighborWeightIteratorBase {

    elementIterType elementIter;
    weightIterType weightIter;

public:
    // The value type of the neighbors (i.e. nodes). Returned by
    // operator*().
    using value_type = std::pair<typename std::iterator_traits<elementIterType>::value_type,
                                 typename std::iterator_traits<weightIterType>::value_type>;

    // Reference to the value_type, required by STL.
    using reference = value_type &;

    // Pointer to the value_type, required by STL.
    using pointer = value_type *;

    // STL iterator category.
    using iterator_category = std::forward_iterator_tag;

    // Signed integer type of the result of subtracting two pointers,
    // required by STL.
    using difference_type = ptrdiff_t;

    // Own type.
    using self = NeighborWeightIteratorBase;

    NeighborWeightIteratorBase(elementIterType elementIter, weightIterType weightIter)
        : elementIter(elementIter), weightIter(weightIter) {}

    /**
     * @brief WARNING: This contructor is required for Python and should not be used as the
     * iterator is not initialized.
     */
    NeighborWeightIteratorBase() : elementIter{}, weightIter{} {}

    NeighborWeightIteratorBase &operator++() {
        ++elementIter;
        ++weightIter;
        return *this;
    }

    NeighborWeightIteratorBase operator++(int) {
        const auto tmp = *this;
        ++(*this);
        return tmp;
    }

    NeighborWeightIteratorBase operator--() {
        --elementIter;
        --weightIter;
        return *this;
    }

    NeighborWeightIteratorBase operator--(int) {
        const auto tmp = *this;
        --(*this);
        return tmp;
    }

    /// The sequences advance in lockstep, so the element iterator alone decides equality.
    bool operator==(const NeighborWeightIteratorBase &rhs) const {
        return elementIter == rhs.elementIter;
    }

    bool operator!=(const NeighborWeightIteratorBase &rhs) const { return !(*this == rhs); }

    value_type operator*() const { return value_type(*elementIter, *weightIter); }
};

/**
 * Class to iterate over the in/out neighbors of a node, including the weights and edge ids
 * given by parallel sequences.
 */
template <typename elementIterType, typename weightIterType, typename idIterType>
class NeighborWeightIdIteratorBase {

    elementIterType elementIter;
    weightIterType weightIter;
    idIterType idIter;

public:
    using value_type = std::tuple<typename std::iterator_traits<elementIterType>::value_type,
                                  typename std::iterator_traits<weightIterType>::value_type,
                                  typename std::iterator_traits<idIterType>::value_type>;

    using reference = value_type &;
    using pointer = value_type *;
    using iterator_category = std::forward_iterator_tag;
    using difference_type = ptrdiff_t;
    using self = NeighborWeightIdIteratorBase;

    NeighborWeightIdIteratorBase(elementIterType elementIter, weightIterType weightIter,
                                 idIterType idIter)
        : elementIter(elementIter), weightIter(weightIter), idIter(idIter) {}

    NeighborWeightIdIteratorBase() : elementIter{}, weightIter{}, idIter{} {}

    NeighborWeightIdIteratorBase &operator++() {
        ++elementIter;
        ++weightIter;
        ++idIter;
        return *this;
    }

    NeighborWeightIdIteratorBase operator++(int) {
        const auto tmp = *this;
        ++(*this);
        return tmp;
    }

    /// The sequences advance in lockstep, so the element iterator alone decides equality.
    bool operator==(const NeighborWeightIdIteratorBase &rhs) const {
        return elementIter == rhs.elementIter;
    }

    bool operator!=(const NeighborWeightIdIteratorBase &rhs) const { return !(*this == rhs); }

    value_type operator*() const { return value_type(*elementIter, *weightIter, *idIter); }
};

class InducedSubgraphView;

/**
 * Class to iterate over the subset members among one node's neighbors in an induced subgraph view.
 *
 * Positioned by an index into the base graph's neighborhood rather than by an iterator into it, so
 * that the type can be named without the view. A view is an arm of ReferenceGraph, and
 * ReferenceGraph::NeighborIterator lists one iterator type per arm; were this built out of the
 * base's neighbor iterator, that alias would be defined in terms of itself. The cost is an
 * out-of-line call per step, which is why the callback iteration does not use this.
 *
 * Dereferencing yields local ids, so a caller never sees the base graph's numbering.
 */
template <bool Weighted>
class ViewNeighborIterator {
public:
    using value_type = std::conditional_t<Weighted, std::pair<node, edgeweight>, node>;
    using reference = value_type;
    using pointer = void;
    using iterator_category = std::forward_iterator_tag;
    using difference_type = ptrdiff_t;

    ViewNeighborIterator() = default;

    /// @a i must be the position of a subset member, or none for the end.
    ViewNeighborIterator(const InducedSubgraphView *view, node baseNode, bool incoming, index i)
        : view(view), baseNode(baseNode), i(i), incoming(incoming) {}

    value_type operator*() const;
    ViewNeighborIterator &operator++();

    ViewNeighborIterator operator++(int) {
        auto tmp = *this;
        ++*this;
        return tmp;
    }

    /// Only iterators over the same neighborhood are comparable, so the position decides.
    bool operator==(const ViewNeighborIterator &other) const noexcept { return i == other.i; }
    bool operator!=(const ViewNeighborIterator &other) const noexcept { return i != other.i; }

private:
    const InducedSubgraphView *view = nullptr;
    node baseNode = none;
    index i = none;
    bool incoming = false;
};

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_NEIGHBOR_ITERATORS_HPP_
