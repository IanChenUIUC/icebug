#!/usr/bin/env python3

import argparse
import os
from pathlib import Path

import networkit as nk
from networkit.scd import ShellStruct


def build_demo_graph():
    graph = nk.Graph(7, weighted=False, directed=False)
    for u, v in [(0, 1), (1, 2), (0, 2)]:
        graph.addEdge(u, v)
    for u, v in [(3, 4), (4, 5), (3, 5), (3, 6), (4, 6), (5, 6)]:
        graph.addEdge(u, v)
    graph.addEdge(2, 3)
    return graph


def main():
    components = Path("output") / "shellstruct.components.parquet"
    tree = Path("output") / "shellstruct.tree.parquet"

    graph = build_demo_graph()
    shell = ShellStruct(graph)

    if components.exists() and tree.exists():
        shell.load(components, tree)
    else:
        shell.build()

    seed = {1, 2, 3}
    comm = shell.expandOneCommunity(seed)
    print(comm)

    if not components.exists() or not tree.exists():
        shell.save(components, tree, compression="ZSTD")


if __name__ == "__main__":
    main()
