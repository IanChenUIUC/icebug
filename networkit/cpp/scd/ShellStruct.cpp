/*
 * ShellStruct.cpp
 *
 *  Created on: 06.05.2026
 *      Author: Ian Chen (ianchen3@illinois.edu)
 */

#include <iterator>
#include <span>
#include <stdexcept>

#include <arrow/array/builder_primitive.h>
#include <networkit/centrality/CoreDecomposition.hpp>
#include <networkit/graph/BFS.hpp>
#include <networkit/scd/SelectiveCommunityDetector.hpp>
#include <networkit/scd/ShellStruct.hpp>
#include <networkit/structures/ConcurrentUnionFind.hpp>
#include <networkit/structures/LeastCommonAncestor.hpp>

namespace NetworKit {

template <typename LocalState, typename ProcessFunc, typename CombineFunc>
static void forNeighborsOfSet(const Graph &G, std::span<const node> S, ProcessFunc F,
                              CombineFunc Combine) {
#pragma omp parallel
    {
        LocalState local_state;
#pragma omp for schedule(guided)
        for (size_t i = 0; i < S.size(); ++i) {
            node v = S[i];
            G.forNeighborsOf(v, [&](node neighbor) { F(v, neighbor, local_state); });
        }
#pragma omp critical
        Combine(local_state);
    }
}

template <typename KeyFunc, typename ValueFunc>
static void append_to_csr(std::vector<index> &indptr, std::vector<node> &indices,
                          std::span<const node> items, size_t num_keys, KeyFunc key_func,
                          ValueFunc val_func) {
    if (num_keys == 0)
        return;

    std::vector<size_t> counts(num_keys, 0);
    for (const auto &item : items)
        counts[key_func(item)]++;

    size_t start_pos = indices.size();
    indices.resize(start_pos + std::size(items));

    std::vector<size_t> write_offsets(num_keys);
    index current_offset = start_pos;

    for (size_t i = 0; i < num_keys; ++i) {
        write_offsets[i] = current_offset;
        current_offset += counts[i];
        indptr.push_back(current_offset);
    }

    for (const auto &item : items) {
        size_t key = key_func(item);
        indices[write_offsets[key]++] = val_func(item);
    }
}

ShellStruct::ShellStruct(const Graph &g) : SelectiveCommunityDetector(g) {}

void ShellStruct::build() {
    NetworKit::CoreDecomposition coredecomp(*g);
    coredecomp.run();

    const std::vector<double> &scores = coredecomp.scores();
    auto *heap_vec = new std::vector<uint64_t>();
    heap_vec->reserve(scores.size());
    for (double s : scores) {
        heap_vec->push_back(static_cast<uint64_t>(s));
    }

    index bytes = heap_vec->size() * sizeof(uint64_t);
    auto buffer = std::shared_ptr<arrow::Buffer>(
        new arrow::Buffer(reinterpret_cast<const uint8_t *>(heap_vec->data()), bytes),
        [heap_vec](arrow::Buffer *b) {
            delete b;
            delete heap_vec;
        });

    auto arrow_cores = std::make_shared<arrow::UInt64Array>(heap_vec->size(), buffer);
    build(arrow_cores);
}

void ShellStruct::build(const std::shared_ptr<arrow::UInt64Array> &coredecomp) {
    index n = g->upperNodeIdBound();

    index max_k = 0;
    for (node i = 0; i < coredecomp->length(); ++i)
        max_k = std::max(max_k, coredecomp->Value(i));

    std::vector<index> shell_indptr(max_k + 2, 0);
    for (node i = 0; i < n; ++i)
        shell_indptr[coredecomp->Value(i)]++;

    index current_offset = 0;
    for (index k = 0; k <= max_k; ++k) {
        index count = shell_indptr[k];
        shell_indptr[k] = current_offset;
        current_offset += count;
    }
    shell_indptr[max_k + 1] = current_offset;

    std::vector<node> shell_nodes(n);
    std::vector<index> insert_offsets = shell_indptr;
    for (node i = 0; i < n; ++i) {
        index k = coredecomp->Value(i);
        shell_nodes[insert_offsets[k]++] = i;
    }

    count next_id = 0;
    std::vector<node> tree_ids(n, none), tree_v, tree_c;
    std::vector<index> tree_v_indptr = {0}, tree_c_indptr = {0};
    std::vector<index> coreness_vec, assignment_vec(n, none);
    ConcurrentUnionFind dsu(n);

    std::vector<node> new_ids(n, none);

    for (index k = max_k; k > 0; --k) {
        size_t shell_size = shell_indptr[k + 1] - shell_indptr[k];
        if (shell_size == 0)
            continue;
        std::span<const node> V_k(shell_nodes.data() + shell_indptr[k], shell_size);

        // Phase 2a: Identifies roots of neighboring components in higher k-shells.
        std::vector<node> touched;
        forNeighborsOfSet<std::vector<node>>(
            *g, V_k,
            [&](node v, node neighbor, std::vector<node> &local_touched) {
                index neighbor_k = coredecomp->Value(neighbor);
                if (neighbor_k > k) {
                    node old_root = dsu.find(neighbor);
                    if (tree_ids[old_root] != none)
                        local_touched.push_back(old_root);
                }
            },
            [&](const std::vector<node> &local_touched) {
                touched.insert(touched.end(), local_touched.begin(), local_touched.end());
            });

        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());

        // Phase 2b: Contracts k-shell into components.
        forNeighborsOfSet<int>(
            *g, V_k,
            [&](node v, node neighbor, int &) {
                index neighbor_k = coredecomp->Value(neighbor);
                if (neighbor_k >= k)
                    dsu.merge(v, neighbor);
            },
            [&](const int &) {});

        // Phase 3: Assigns new hierarchical tree node IDs to newly formed disjoint components.
        std::vector<node> new_roots;
        for (index i = 0; i < shell_size; ++i) {
            node root = dsu.find(V_k[i]);
            if (new_ids[root] == none) {
                new_roots.push_back(root);
                new_ids[root] = next_id++;
                coreness_vec.push_back(k);
            }
        }

#pragma omp parallel for schedule(static)
        for (index i = 0; i < shell_size; ++i)
            assignment_vec[V_k[i]] = new_ids[dsu.find(V_k[i])];

        // Phases 4: Builds the tree CSR (and the inverse assignment)
        count num_new_trees = new_roots.size();
        count base_tree_id = next_id - num_new_trees;
        append_to_csr(
            tree_v_indptr, tree_v, V_k, num_new_trees,
            [&](node v) { return new_ids[dsu.find(v)] - base_tree_id; }, [&](node v) { return v; });
        append_to_csr(
            tree_c_indptr, tree_c, touched, num_new_trees,
            [&](node v) { return new_ids[dsu.find(v)] - base_tree_id; },
            [&](node v) { return tree_ids[v]; });

        // Phase 5: Cleanup
        for (node old_root : touched)
            tree_ids[old_root] = none;
        for (node root : new_roots) {
            tree_ids[root] = new_ids[root];
            new_ids[root] = none;
        }
    }

    // Phase 6: ensure tree is connected
    size_t shell_size_0 = shell_indptr[1] - shell_indptr[0];
    std::span<const node> V_0(shell_nodes.data() + shell_indptr[0], shell_size_0);
    std::vector<node> forest_roots;
    for (node id : tree_ids)
        if (id != none)
            forest_roots.push_back(id);
    node root = next_id++;
    coreness_vec.push_back(0);

#pragma omp parallel for schedule(static)
    for (size_t i = 0; i < shell_size_0; ++i)
        assignment_vec[V_0[i]] = root;

    append_to_csr(
        tree_v_indptr, tree_v, V_0, 1, [&](node) { return 0; }, [&](node v) { return v; });
    append_to_csr(
        tree_c_indptr, tree_c, forest_roots, 1, [&](node) { return 0; },
        [&](node child) { return child; });

    // Final Phase: convert to member types (LLM generated)

    tree = GraphR(next_id, true, std::move(tree_c), std::move(tree_c_indptr));
    lca = std::make_unique<NetworKit::LeastCommonAncestor>(*tree, root);

    auto make_arrow_array = [](auto vec) {
        using T = typename decltype(vec)::value_type;
        int64_t length = vec.size();
        int64_t byte_size = length * sizeof(T);

        auto *heap_vec = new std::vector<T>(std::move(vec));
        auto buffer = std::shared_ptr<arrow::Buffer>(
            new arrow::Buffer(reinterpret_cast<const uint8_t *>(heap_vec->data()), byte_size),
            [heap_vec](arrow::Buffer *b) {
                delete b;
                delete heap_vec;
            });

        if constexpr (sizeof(T) == 4 && std::is_signed_v<T>) {
            return std::make_shared<arrow::Int32Array>(length, buffer);
        } else if constexpr (sizeof(T) == 8 && std::is_signed_v<T>) {
            return std::make_shared<arrow::Int64Array>(length, buffer);
        } else if constexpr (sizeof(T) == 4 && std::is_unsigned_v<T>) {
            return std::make_shared<arrow::UInt32Array>(length, buffer);
        } else if constexpr (sizeof(T) == 8 && std::is_unsigned_v<T>) {
            return std::make_shared<arrow::UInt64Array>(length, buffer);
        } else {
            throw std::runtime_error("Unsupported Arrow integer type");
        }
    };

    std::vector<int64_t> offsets(tree_v_indptr.begin(), tree_v_indptr.end());
    auto indptr_arr = make_arrow_array(std::move(offsets));
    auto indices_arr = make_arrow_array(std::move(tree_v));

    vertices = std::static_pointer_cast<arrow::LargeListArray>(
        arrow::LargeListArray::FromArrays(*indptr_arr, *indices_arr).ValueOrDie());
    assignment = make_arrow_array(std::move(assignment_vec));
    coreness = make_arrow_array(std::move(coreness_vec));

    // mark as built
    built = true;
}

std::set<node> ShellStruct::expandOneCommunity(const std::set<node> &s) {
    if (!built)
        throw std::invalid_argument("Need to build() or load() the shell struct");

    std::vector<node> output;
    if (s.empty())
        return std::set<node>(output.begin(), output.end());

    std::vector<node> query;
    query.reserve(s.size());
    for (node u : s)
        query.push_back(assignment->Value(u));
    node ancestor = lca->Query(std::vector<node>(query.begin(), query.end()));

    const uint64_t *raw_v =
        std::static_pointer_cast<arrow::UInt64Array>(vertices->values())->raw_values();
    const int64_t *raw_offsets = vertices->raw_value_offsets();

    Traversal::BFSfrom(*tree, ancestor, [&](node u) {
        output.insert(output.end(), raw_v + raw_offsets[u], raw_v + raw_offsets[u + 1]);
    });

    return std::set<node>(output.begin(), output.end());
}

// TODO
// void ShellStruct::save(const std::string &components_path, const std::string &tree_path,
//                        const std::string &compression) const {
//     INFO("calling ShellStruct::save");
// }

// TODO
// void ShellStruct::load(const std::string &components_path, const std::string &tree_path) {
//     INFO("calling ShellStruct::save");
// }

} // namespace NetworKit
