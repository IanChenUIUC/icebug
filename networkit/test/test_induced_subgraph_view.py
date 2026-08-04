#!/usr/bin/env python3
import copy
import gc
import unittest

import pyarrow as pa

import networkit as nk


def buildBase(n, weighted, directed, seed=1):
	"""A connected-ish random base graph, with reproducible weights when weighted."""
	nk.setSeed(seed, True)
	G = nk.generators.ErdosRenyiGenerator(n, 0.25, directed).generate()
	if not weighted:
		return G
	G = nk.graphtools.toWeighted(G)
	for u, v in G.iterEdges():
		G.setWeight(u, v, 1.0 + ((u * 7 + v * 13) % 11) / 4.0)
	return G


def toCSR(G):
	"""
	The same graph as a CSR-backed (read-only) Graph, via the zero-copy Arrow path.

	Neighborhoods are sorted: GraphR binary-searches them, and nothing checks the invariant.
	"""
	n = G.upperNodeIdBound()
	indices, indptr, weights = [], [0], []
	for u in range(n):
		for v in sorted(G.iterNeighbors(u)):
			indices.append(v)
			weights.append(G.weight(u, v))
		indptr.append(len(indices))

	outIndices = pa.array(indices, type=pa.uint64())
	outIndptr = pa.array(indptr, type=pa.uint64())
	outWeights = pa.array(weights, type=pa.float64()) if G.isWeighted() else None

	if not G.isDirected():
		return nk.Graph.fromCSR(n, False, outIndices, outIndptr, out_weights=outWeights)

	inIndices, inIndptr, inWeights = [], [0], []
	for u in range(n):
		for v in sorted(G.iterInNeighbors(u)):
			inIndices.append(v)
			inWeights.append(G.weight(v, u))
		inIndptr.append(len(inIndices))
	return nk.Graph.fromCSR(
		n, True, outIndices, outIndptr,
		pa.array(inIndices, type=pa.uint64()), pa.array(inIndptr, type=pa.uint64()),
		outWeights, pa.array(inWeights, type=pa.float64()) if G.isWeighted() else None)


def edgeMultiset(G):
	"""
	Every edge as (u, v, weight), oriented low-to-high when undirected.

	Walks neighborhoods rather than G.iterEdges(), which is unsupported on a view -- see
	testEdgeIterationIsUnsupported.
	"""
	edges = []
	for u in G.iterNodes():
		for v in G.iterNeighbors(u):
			a, b = (u, v) if G.isDirected() else (min(u, v), max(u, v))
			edges.append((a, b, G.weight(u, v)))
	return sorted(edges)


class TestInducedSubgraphView(unittest.TestCase):

	def assertGraphsEqual(self, expected, actual):
		self.assertEqual(expected.isWeighted(), actual.isWeighted())
		self.assertEqual(expected.isDirected(), actual.isDirected())
		self.assertEqual(expected.numberOfNodes(), actual.numberOfNodes())
		self.assertEqual(expected.numberOfEdges(), actual.numberOfEdges())
		self.assertEqual(expected.upperNodeIdBound(), actual.upperNodeIdBound())
		self.assertEqual(sorted(expected.iterNodes()), sorted(actual.iterNodes()))
		self.assertAlmostEqual(expected.totalEdgeWeight(), actual.totalEdgeWeight())
		self.assertEqual(edgeMultiset(expected), edgeMultiset(actual))

	def subsets(self, G):
		nodes = sorted(G.iterNodes())
		return [[], nodes[:3], nodes[::2], nodes]

	def testAgainstSubgraphFromNodes(self):
		for weighted in [False, True]:
			for directed in [False, True]:
				G = buildBase(30, weighted, directed)
				for compact in [False, True]:
					for subset in self.subsets(G):
						V = nk.graph.InducedSubgraphView(G, compact)
						V.addNodes(subset)
						self.assertEqual(V.getNodeSubset(), subset)
						self.assertEqual(V.compact, compact)
						self.assertGraphsEqual(
							nk.graphtools.subgraphFromNodes(G, subset, compact), V.asGraph())

	def testCsrBase(self):
		for weighted in [False, True]:
			for directed in [False, True]:
				G = buildBase(30, weighted, directed)
				R = toCSR(G)
				self.assertGraphsEqual(G, R)
				for compact in [False, True]:
					for subset in self.subsets(G):
						V = nk.graph.InducedSubgraphView(R, compact)
						V.addNodes(subset)
						self.assertGraphsEqual(
							nk.graphtools.subgraphFromNodes(G, subset, compact), V.asGraph())

	def testIncrementalGrowAndShrink(self):
		for directed in [False, True]:
			G = buildBase(40, True, directed)
			nodes = sorted(G.iterNodes())
			for compact in [False, True]:
				V = nk.graph.InducedSubgraphView(G, compact)
				S = V.asGraph()

				for i in range(0, len(nodes), 7):
					V.addNodes(nodes[i:i + 7])
					self.assertGraphsEqual(
						nk.graphtools.subgraphFromNodes(G, nodes[:i + 7], compact), S)

				for i in range(0, len(nodes), 7):
					V.removeNodes(nodes[i:i + 7])
					self.assertGraphsEqual(
						nk.graphtools.subgraphFromNodes(G, nodes[i + 7:], compact), S)

				self.assertEqual(V.getNodeSubset(), [])

	def testAddAndRemoveSingleNode(self):
		G = buildBase(20, False, False)
		V = nk.graph.InducedSubgraphView(G)
		for u in [4, 4, 9, 1]:
			V.addNode(u)
		self.assertEqual(V.getNodeSubset(), [1, 4, 9])
		V.removeNode(4)
		V.removeNode(4)
		self.assertEqual(V.getNodeSubset(), [1, 9])

	def testRealize(self):
		for directed in [False, True]:
			G = buildBase(30, True, directed)
			subset = sorted(G.iterNodes())[::2]
			for viewCompact in [False, True]:
				for realizeCompact in [False, True]:
					V = nk.graph.InducedSubgraphView(G, viewCompact)
					V.addNodes(subset)
					self.assertGraphsEqual(
						nk.graphtools.subgraphFromNodes(G, subset, realizeCompact),
						V.realize(realizeCompact))

	def testRealizeIsWritableAndIndependent(self):
		G = buildBase(20, False, False)
		V = nk.graph.InducedSubgraphView(G, True)
		V.addNodes([0, 1, 2, 3])
		H = V.realize(True)
		H.addNode()
		V.removeNode(0)
		self.assertEqual(H.numberOfNodes(), 5)

	def testNeighborIteration(self):
		G = buildBase(30, True, True)
		subset = sorted(G.iterNodes())[::2]
		V = nk.graph.InducedSubgraphView(G)
		V.addNodes(subset)
		S = V.asGraph()
		ref = nk.graphtools.subgraphFromNodes(G, subset, False)

		for u in ref.iterNodes():
			self.assertEqual(sorted(S.iterNeighbors(u)), sorted(ref.iterNeighbors(u)))
			self.assertEqual(sorted(S.iterInNeighbors(u)), sorted(ref.iterInNeighbors(u)))
			self.assertEqual(sorted(S.iterNeighborsWeights(u)),
			                 sorted(ref.iterNeighborsWeights(u)))
			self.assertEqual(sorted(S.iterInNeighborsWeights(u)),
			                 sorted(ref.iterInNeighborsWeights(u)))
			self.assertEqual(S.degree(u), ref.degree(u))
			self.assertEqual(S.degreeIn(u), ref.degreeIn(u))

	def testCoreDecomposition(self):
		G = buildBase(60, False, False)
		subset = sorted(G.iterNodes())[:40]
		V = nk.graph.InducedSubgraphView(G, True)
		V.addNodes(subset)

		viewScores = nk.centrality.CoreDecomposition(V.asGraph()).run().scores()
		refScores = nk.centrality.CoreDecomposition(V.realize(True)).run().scores()
		self.assertEqual(viewScores, refScores)

	def testEdgeIterationIsUnsupported(self):
		# A view stores no edges, so the index-stepping edge iterator has nothing to index. Drop
		# this test once ReferenceGraph gets an edge iterator built on its neighbor iterator.
		G = buildBase(20, True, False)
		V = nk.graph.InducedSubgraphView(G)
		V.addNodes(sorted(G.iterNodes()))
		S = V.asGraph()
		self.assertGreater(S.numberOfEdges(), 0)
		with self.assertRaises(RuntimeError):
			list(S.iterEdges())
		with self.assertRaises(RuntimeError):
			list(S.iterEdgesWeights())

	def testViewOfViewIsRejected(self):
		G = buildBase(20, False, False)
		V = nk.graph.InducedSubgraphView(G)
		with self.assertRaises(TypeError):
			nk.graph.InducedSubgraphView(V.asGraph())

	def testSeatedGraphIsReadOnly(self):
		G = buildBase(20, False, False)
		V = nk.graph.InducedSubgraphView(G)
		V.addNodes([0, 1, 2])
		S = V.asGraph()
		with self.assertRaises(RuntimeError):
			S.addEdge(0, 1)
		with self.assertRaises(RuntimeError):
			S.removeNode(0)

	def testSeatedGraphKeepsTheBaseAlive(self):
		V = nk.graph.InducedSubgraphView(buildBase(20, False, False), True)
		V.addNodes([0, 1, 2, 3])
		S = V.asGraph()
		del V
		gc.collect()
		self.assertEqual(S.numberOfNodes(), 4)
		self.assertEqual(sorted(S.iterNodes()), [0, 1, 2, 3])

	def testCopyMaterializes(self):
		G = buildBase(20, False, False)
		V = nk.graph.InducedSubgraphView(G, True)
		V.addNodes([0, 1, 2, 3])
		S = copy.copy(V.asGraph())
		V.removeNode(0)
		self.assertEqual(S.numberOfNodes(), 4)
		S.addEdge(0, 1, checkMultiEdge=True)


if __name__ == "__main__":
	unittest.main()
