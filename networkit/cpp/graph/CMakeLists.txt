networkit_add_module(graph
    GraphR.cpp
    InducedSubgraphView.cpp
    GraphBuilder.cpp
    GraphTools.cpp
    GraphW.cpp
    InducedSubgraphView.cpp
    KruskalMSF.cpp
    PrimMSF.cpp
    RandomMaximumSpanningForest.cpp
    SpanningForest.cpp
    TopologicalSort.cpp
    UnionMaximumSpanningForest.cpp
    )

networkit_module_link_modules(graph
    auxiliary structures)

add_subdirectory(test)

