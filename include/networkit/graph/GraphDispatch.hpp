/*
 * GraphDispatch.hpp
 *
 *  Created on: 07.20.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 *
 * The single place that recovers a concrete graph type from a `const Graph &`
 * so an algorithm can run over the static-dispatch iteration API instead of the
 * base class's virtual std::function path. An algorithm opts in by routing its
 * run body through `visitConcreteGraph`; a new statically-dispatchable type is
 * added here once (one `dynamic_cast` line) and every such algorithm picks it
 * up. The fallback preserves the unchanged `const Graph &` behavior.
 */

#ifndef NETWORKIT_GRAPH_GRAPH_DISPATCH_HPP_
#define NETWORKIT_GRAPH_GRAPH_DISPATCH_HPP_

#include <networkit/graph/Graph.hpp>
#include <networkit/graph/GraphW.hpp>

namespace NetworKit {

template <typename Fn>
inline void visitConcreteGraph(const Graph &G, Fn &&fn) {
    if (const auto *w = dynamic_cast<const GraphW *>(&G)) {
        fn(*w);
        return;
    }
    fn(G);
}

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_GRAPH_DISPATCH_HPP_
