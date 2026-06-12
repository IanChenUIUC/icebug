#!/usr/bin/env python3

from pathlib import Path

import networkit as nk
from networkit.scd import SteinerKCore, LocalKCore

nk.setLogLevel("INFO")


def build_demo_graph():
    graph = nk.Graph(11, weighted=False, directed=False)
    for u, v in [(0, 1), (0, 2), (0, 3), (1, 2), (1, 3), (2, 3)]:
        graph.addEdge(u, v)
    for u, v in [(4, 5), (4, 6), (5, 6)]:
        graph.addEdge(u, v)
    for u, v in [(7, 8), (7, 9), (7, 10), (8, 9), (8, 10), (9, 10)]:
        graph.addEdge(u, v)
    graph.addEdge(3, 4)
    graph.addEdge(6, 7)

    coreness = [3, 3, 3, 3, 2, 2, 2, 3, 3, 3, 3]
    return graph, coreness


def main():
    graph, coreness = build_demo_graph()

    print("====== Running SteinerKCore ========")
    commsearch = SteinerKCore(graph, coreness)

    seed = {0, 1, 2}
    print(f"{seed=}:", commsearch.expandOneCommunity(seed))
    seed = {4}
    print(f"{seed=}:", commsearch.expandOneCommunity(seed))
    seed = {2, 9}
    print(f"{seed=}:", commsearch.expandOneCommunity(seed))

    comms = commsearch.run([{0, 1, 2}, {4}, {2, 9}])
    print("Running all three as a batch:", comms)

    print("====== Running LocalKCore ========")
    commsearch = LocalKCore(graph)

    seed = {0, 1, 2}
    print(f"{seed=}:", commsearch.expandOneCommunity(seed))
    seed = {4}
    print(f"{seed=}:", commsearch.expandOneCommunity(seed))
    seed = {2, 9}
    print(f"{seed=}:", commsearch.expandOneCommunity(seed))


if __name__ == "__main__":
    main()
