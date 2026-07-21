/*
 * ShellStruct.hpp
 *
 *  Created on: 06.05.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_SHELL_STRUCT_HPP_
#define NETWORKIT_SCD_SHELL_STRUCT_HPP_

#include <concepts>
#include <functional>
#include <optional>

#include <networkit/graph/GraphR.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>
#include <networkit/structures/LeastCommonAncestor.hpp>

#include "networkit/graph/BFS.hpp"
#include <arrow/array/array_nested.h>
#include <arrow/type_fwd.h>
#include <networkit/Globals.hpp>
#include <networkit/graph/Graph.hpp>

namespace NetworKit {

/**
 * The ShellStruct index for fast online k-core community search.
 *
 * This is an implementation of the algorithm proposed in:
 *
 * Fang, Y., Cheng, C. K., Luo, S., & Hu, J. (2016).
 * Effective community search for large attributed graphs.
 * Proceedings of the VLDB Endowment.
 * https://doi.org/10.14778/2994509.2994538
 */
class ShellStruct : public SelectiveCommunityDetector {

private:
    std::shared_ptr<arrow::UInt64Array> assignment;  // vertices -> tree nodes
    std::shared_ptr<arrow::UInt64Array> coreness;    // tree nodes -> coreness
    std::shared_ptr<arrow::LargeListArray> vertices; // tree nodes -> vertices

    bool built;
    std::optional<GraphR> tree;
    std::shared_ptr<arrow::UInt64Array> treeIndptr;
    std::shared_ptr<arrow::UInt64Array> treeIndices;
    std::unique_ptr<NetworKit::LeastCommonAncestor> lca;

    arrow::Status saveInternal(const std::string &components_path, const std::string &tree_path,
                               const std::string &compression) const;
    arrow::Status loadInternal(const std::string &components_path, const std::string &tree_path);

public:
    ShellStruct(const Graph &g);

    /**
     * Builds the shell struct.
     */
    void build();
    void build(std::span<const count> coredecomp);

    /**
     * @param[in] seeds seed nodes
     *
     * @param[out] community as a set of nodes
     */
    std::set<node> expandOneCommunity(const std::set<node> &s) override;

    /**
     * Finding the score without instantiating the community is much faster.
     * This method does that.
     *
     * @param[in] seeds seed nodes
     *
     * @param[out] the core number of the set
     */
    index score(const std::set<node> &s);

    /**
     * Applies a reduction operator on the output community.
     * This is usually much faster than instantiating the community explicitly.
     *
     * @param[in] seeds seed nodes
     * @param[in] handle takes parameter <code>(node)</code>.
     *
     * @param[out] applying F commutative reduction operator to all v in the community.
     */
    template <typename L>
    void forCommunity(const std::set<node> &s, L handle);

    /**
     * Saves the shellstruct index to disk in .parquet files.
     *
     * @param[in] components_path The assignment of vertices to components.
     * @param[in] tree_path The coreness, vertices, and child structure of the tree.
     */
    void save(const std::string &components_path, const std::string &tree_path,
              const std::string &compression = "ZSTD") const;

    /**
     * Loads the shellstruct index from disk via .parquet files.
     *
     * @param[in] components_path The assignment of vertices to components.
     * @param[in] tree_path The coreness, vertices, and child structure of the tree.
     */
    void load(const std::string &components_path, const std::string &tree_path);

    // inherit method from parent class.
    using SelectiveCommunityDetector::expandOneCommunity;
};

template <typename L>
void ShellStruct::forCommunity(const std::set<node> &s, L handle) {
    if (!built)
        throw std::runtime_error("Need to build() or load() the shell struct");

    if (s.empty())
        return;

    std::vector<node> query;
    query.reserve(s.size());
    for (node u : s)
        query.push_back(assignment->Value(static_cast<int64_t>(u)));

    const uint64_t *raw_v =
        std::static_pointer_cast<arrow::UInt64Array>(vertices->values())->raw_values();
    const int64_t *raw_offsets = vertices->raw_value_offsets();

    node ancestor = lca->Query(std::vector<node>(query.begin(), query.end()));
    Traversal::BFSfrom(*tree, ancestor, [&](node u) {
        for (index i = raw_offsets[u]; i < raw_offsets[u + 1]; ++i)
            handle(raw_v[i]);
    });
}

} /* namespace NetworKit */
#endif // NETWORKIT_SCD_SHELL_STRUCT_HPP_
