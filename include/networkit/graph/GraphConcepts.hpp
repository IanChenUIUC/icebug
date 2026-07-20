/*
 * GraphConcepts.hpp
 *
 *  Created on: 07.19.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 *
 *
 * The public interface a graph class must provide to participate
 * in static-dispatch iteration (via GraphIteratorMixin).
 * Requires expliciting tagging of `isNetworKitGraph`.
 */

#ifndef NETWORKIT_GRAPH_GRAPH_CONCEPTS_HPP_
#define NETWORKIT_GRAPH_GRAPH_CONCEPTS_HPP_

#include <concepts>

#include <networkit/Globals.hpp>

namespace NetworKit {

template <class G>
concept GraphType = requires(const G g, node u) {
    { g.numberOfNodes() } -> std::convertible_to<count>;
    { g.upperNodeIdBound() } -> std::convertible_to<index>;
    { g.hasNode(u) } -> std::convertible_to<bool>;
    { g.degree(u) } -> std::convertible_to<count>;
    { g.isDirected() } -> std::convertible_to<bool>;
    { g.isWeighted() } -> std::convertible_to<bool>;

    requires G::isNetworKitGraph;

    // sequential iteration surface algorithms rely on:
    g.forNodes([](node) {});
    g.forEdges([](node, node) {});
    g.forNeighborsOf(u, [](node) {});
};

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_GRAPH_CONCEPTS_HPP_
