/*
 * GraphIteratorMixin.hpp
 *
 *  Created on: 07.19.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

/*
 * Implementation Details
 * CRTP mixin that synthesizes the static-dispatch edge-iteration API
 * (forEdges / forEdgesOf / forNeighborsOf / forInEdgesOf / forInNeighborsOf /
 * parallelForEdges / parallelSumForEdges) for a concrete graph class, so that
 * code holding the concrete type (or a `template <GraphType G>` parameter)
 * iterates through fully-inlined templates instead of the base class's
 * `std::function`-per-edge virtual path.
 *
 * A `Derived` class opts in by (1) inheriting this mixin, (2) `using` its
 * methods to shadow the base `Graph::` versions, and (3) providing two raw
 * neighbor enumerators, which the mixin composes:
 *
 *   // cb(v, weight, edgeid) per out-edge, no dedup
 *   template <bool weighted, bool edgeIds, class Cb>
 *   void forOutEdgesRaw(node u, Cb cb) const;
 *
 *   // cb(v, weight, edgeid) per in-edge, no dedup
 *   template <bool directed, bool weighted, bool edgeIds, class Cb>
 *   void forInEdgesRaw(node u, Cb cb) const;
 *
 * The `weighted`/`edgeIds` (and `directed`) template bools let the enumerator
 * hoist the weight-fetch / edge-id / container choice to compile time, so
 * nothing branches per edge. The mixin owns the undirected `u <= v` dedup and
 * the callback-argument order, reproducing the base class's semantics exactly
 * (see the equivalence test). `Derived` typically makes these private and grants
 * `friend class GraphIteratorMixin<Derived>;`.
 */

#ifndef NETWORKIT_GRAPH_GRAPH_ITERATOR_MIXIN_HPP_
#define NETWORKIT_GRAPH_GRAPH_ITERATOR_MIXIN_HPP_

#include <networkit/Globals.hpp>
#include <networkit/graph/GraphIteratorTools.hpp>

namespace NetworKit {

template <class Derived>
class GraphIteratorMixin {
    const Derived &self() const { return static_cast<const Derived &>(*this); }

    template <typename F>
    void dispatchFlags(F f) const {
        const bool wt = self().isWeighted();
        const bool eid = self().hasEdgeIds();
        if (self().isDirected()) {
            if (wt) {
                if (eid)
                    f.template operator()<true, true, true>();
                else
                    f.template operator()<true, true, false>();
            } else {
                if (eid)
                    f.template operator()<true, false, true>();
                else
                    f.template operator()<true, false, false>();
            }
        } else {
            if (wt) {
                if (eid)
                    f.template operator()<false, true, true>();
                else
                    f.template operator()<false, true, false>();
            } else {
                if (eid)
                    f.template operator()<false, false, true>();
                else
                    f.template operator()<false, false, false>();
            }
        }
    }

public:
    /** For undirected graphs each edge is visited once as (u, v) with u <= v. */
    template <typename L>
    void forEdges(L handle) const {
        dispatchFlags([&]<bool dir, bool wt, bool eid>() {
            self().forNodes([&](node u) {
                self().template forOutEdgesRaw<wt, eid>(u, [&](node v, edgeweight w, edgeid e) {
                    if constexpr (!dir)
                        if (v < u)
                            return;
                    detail::edgeLambda(handle, u, v, w, e);
                });
            });
        });
    }

    /** Iterate over the outgoing incident edges of u. */
    template <typename L>
    void forEdgesOf(node u, L handle) const {
        dispatchFlags([&]<bool dir, bool wt, bool eid>() {
            self().template forOutEdgesRaw<wt, eid>(
                u, [&](node v, edgeweight w, edgeid e) { detail::edgeLambda(handle, u, v, w, e); });
        });
    }

    /** Iterate over the (out-)neighbors of u. */
    template <typename L>
    void forNeighborsOf(node u, L handle) const {
        forEdgesOf(u, handle);
    }

    /** Iterate over the incoming incident edges of u.
     * The handle receives (thisNode, inNeighbor) for the edge form,
     * or the in-neighbor for the neighbor form
     */
    template <typename L>
    void forInEdgesOf(node u, L handle) const {
        dispatchFlags([&]<bool dir, bool wt, bool eid>() {
            self().template forInEdgesRaw<dir, wt, eid>(u, [&](node vIn, edgeweight w, edgeid e) {
                detail::edgeLambda(handle, u, vIn, w, e);
            });
        });
    }

    /** Iterate over the in-neighbors of u. */
    template <typename L>
    void forInNeighborsOf(node u, L handle) const {
        forInEdgesOf(u, handle);
    }

    /** Parallel iteration over all edges (undirected: each once, u <= v). */
    template <typename L>
    void parallelForEdges(L handle) const {
        dispatchFlags([&]<bool dir, bool wt, bool eid>() {
            const auto z = static_cast<omp_index>(self().upperNodeIdBound());
#pragma omp parallel for schedule(guided)
            for (omp_index u = 0; u < z; ++u) {
                if (!self().hasNode(static_cast<node>(u)))
                    continue;
                self().template forOutEdgesRaw<wt, eid>(
                    static_cast<node>(u), [&](node v, edgeweight w, edgeid e) {
                        if constexpr (!dir)
                            if (v < static_cast<node>(u))
                                return;
                        detail::edgeLambda(handle, static_cast<node>(u), v, w, e);
                    });
            }
        });
    }

    /** Parallel reduction (+) over the values returned by the handle for all
     *  edges (undirected: each once, u <= v). */
    template <typename L>
    double parallelSumForEdges(L handle) const {
        double sum = 0.0;
        dispatchFlags([&]<bool dir, bool wt, bool eid>() {
            const auto z = static_cast<omp_index>(self().upperNodeIdBound());
#pragma omp parallel for reduction(+ : sum) schedule(guided)
            for (omp_index u = 0; u < z; ++u) {
                if (!self().hasNode(static_cast<node>(u)))
                    continue;
                self().template forOutEdgesRaw<wt, eid>(
                    static_cast<node>(u), [&](node v, edgeweight w, edgeid e) {
                        if constexpr (!dir)
                            if (v < static_cast<node>(u))
                                return;
                        sum += detail::edgeLambda(handle, static_cast<node>(u), v, w, e);
                    });
            }
        });
        return sum;
    }
};

} // namespace NetworKit

#endif // NETWORKIT_GRAPH_GRAPH_ITERATOR_MIXIN_HPP_
