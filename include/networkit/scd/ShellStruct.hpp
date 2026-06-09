/*
 * ShellStruct.hpp
 *
 *  Created on: 06.05.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#ifndef NETWORKIT_SCD_SHELL_STRUCT_HPP_
#define NETWORKIT_SCD_SHELL_STRUCT_HPP_

#include <optional>
#include <networkit/graph/GraphR.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>
#include <networkit/structures/LeastCommonAncestor.hpp>

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
    std::shared_ptr<arrow::LargeListArray> vertices; // tree nodes -> vertices
    std::shared_ptr<arrow::UInt64Array> assignment;  // vertices -> tree nodes
    std::shared_ptr<arrow::UInt64Array> coreness;    // tree nodes -> coreness

    bool built;
    std::optional<GraphR> tree;
    std::unique_ptr<NetworKit::LeastCommonAncestor> lca;

    arrow::Status saveInternal(const std::string &components_path, const std::string &tree_path);
    arrow::Status loadInternal(const std::string &components_path, const std::string &tree_path);

public:
    ShellStruct(const Graph &g);

    /**
     * Builds the shell struct.
     */
    void build();
    void build(const std::shared_ptr<arrow::UInt64Array> &coredecomp);

    /**
     * @param[in] seeds seed nodes
     *
     * @param[out] community as a set of nodes
     */
    std::set<node> expandOneCommunity(const std::set<node> &s) override;

    /**
     * @param[in]
     */
    void save(const std::string &components_path, const std::string &tree_path,
              const std::string &compression = "ZSTD") const;

    /**
     *
     * @param[in]
     */
    void load(const std::string &components_path, const std::string &tree_path);

    // inherit method from parent class.
    using SelectiveCommunityDetector::expandOneCommunity;
};

} /* namespace NetworKit */
#endif // NETWORKIT_SCD_SHELL_STRUCT_HPP_
