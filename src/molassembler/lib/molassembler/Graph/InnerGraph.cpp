/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#include "molassembler/Graph/InnerGraph.h"

#include "boost/graph/biconnected_components.hpp"
#include "boost/graph/connected_components.hpp"
#include "boost/optional.hpp"

#include "temple/Functional.h"

namespace Scine {

namespace molassembler {

/* Constructors */
InnerGraph::InnerGraph() = default;
InnerGraph::InnerGraph(const InnerGraph::Vertex N) : _graph {N} {}

/* Modifiers */
InnerGraph::Edge InnerGraph::addEdge(const Vertex a, const Vertex b, const BondType bondType) {
  /* We have to be careful here since the edge list for a vector is not a set
   * (see BGLType).  Check if there is already such an edge before adding it.
   */
  auto existingEdgePair = boost::edge(a, b, _graph);
  if(existingEdgePair.second) {
    throw std::logic_error("Edge already exists!");
  }

  // Invalidate the cache only after all throwing conditions
  _unchangedSinceNotification = false;

  auto newBondPair = boost::add_edge(a, b, _graph);

  // The bond may not yet exist
  assert(newBondPair.second);
  _graph[newBondPair.first].bondType = bondType;

  return newBondPair.first;
}

InnerGraph::Vertex InnerGraph::addVertex(const Scine::Utils::ElementType elementType) {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  InnerGraph::Vertex newVertex = boost::add_vertex(_graph);
  _graph[newVertex].elementType = elementType;
  return newVertex;
}

void InnerGraph::applyPermutation(const std::vector<Vertex>& permutation) {
  BGLType transformedGraph(boost::num_vertices(_graph));

  /*boost::copy_graph(
    _graph,
    transformedGraph,
    boost::vertex_index_map(
      boost::make_function_property_map<InnerGraph::Vertex, InnerGraph::Vertex>(
        [&permutation](const InnerGraph::Vertex a) -> const InnerGraph::Vertex& {
          return permutation.at(a);
        }
      )
    )
  );*/
  /* I failed to get copy_graph to perform the permutation for me, so we copy
   * manually:
   */
  for(Vertex i : boost::make_iterator_range(vertices())) {
    transformedGraph[permutation.at(i)].elementType = _graph[i].elementType;
  }

  for(Edge e : boost::make_iterator_range(edges())) {
    auto newBondPair = boost::add_edge(
      permutation.at(boost::source(e, _graph)),
      permutation.at(boost::target(e, _graph)),
      transformedGraph
    );

    transformedGraph[newBondPair.first].bondType = _graph[e].bondType;
  }

  std::swap(_graph, transformedGraph);
}

BondType& InnerGraph::bondType(const InnerGraph::Edge& edge) {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  return _graph[edge].bondType;
}

void InnerGraph::clearVertex(Vertex a) {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  boost::clear_vertex(a, _graph);
}

void InnerGraph::notifyPropertiesCached() const {
  _unchangedSinceNotification = true;
}

void InnerGraph::removeEdge(const Edge& e) {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  boost::remove_edge(e, _graph);
}

void InnerGraph::removeVertex(Vertex a) {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  boost::remove_vertex(a, _graph);
}

Scine::Utils::ElementType& InnerGraph::elementType(const Vertex a) {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  return _graph[a].elementType;
}

InnerGraph::BGLType& InnerGraph::bgl() {
  // Invalidate the cache values
  _unchangedSinceNotification = false;

  return _graph;
}

/* Information */

bool InnerGraph::canRemove(const Vertex a) const {
  // A molecule is by definition at least two atoms!
  if(N() == 2) {
    return false;
  }

  // Removable if the vertex is not an articulation vertex
  return _removalSafetyData().articulationVertices.count(a) == 0;
}

bool InnerGraph::canRemove(const Edge& edge) const {
  // Removable if the edge is not a bridge
  return _removalSafetyData().bridges.count(edge) == 0;
}

/* Information */
unsigned InnerGraph::connectedComponents() const {
  std::vector<unsigned> componentMap(N());
  return boost::connected_components(_graph, &componentMap[0]);
}

unsigned InnerGraph::connectedComponents(std::vector<unsigned>& componentMap) const {
  const Vertex size = N();

  if(componentMap.size() != size) {
    componentMap.resize(size);
  }
  return boost::connected_components(_graph, &componentMap[0]);
}

BondType InnerGraph::bondType(const InnerGraph::Edge& edge) const {
  return _graph[edge].bondType;
}

Scine::Utils::ElementType InnerGraph::elementType(const Vertex a) const {
  return _graph[a].elementType;
}

InnerGraph::Edge InnerGraph::edge(const Vertex a, const Vertex b) const {
  auto edge = boost::edge(a, b, _graph);
  assert(edge.second);
  return edge.first;
}

boost::optional<InnerGraph::Edge> InnerGraph::edgeOption(const Vertex a, const Vertex b) const {
  auto edge = boost::edge(a, b, _graph);

  if(edge.second) {
    return edge.first;
  }

  return boost::none;
}

InnerGraph::Vertex InnerGraph::source(const InnerGraph::Edge& edge) const {
  return boost::source(edge, _graph);
}

InnerGraph::Vertex InnerGraph::target(const InnerGraph::Edge& edge) const {
  return boost::target(edge, _graph);
}

InnerGraph::Vertex InnerGraph::degree(const InnerGraph::Vertex a) const {
  return boost::out_degree(a, _graph);
}

InnerGraph::Vertex InnerGraph::N() const {
  return boost::num_vertices(_graph);
}

InnerGraph::Vertex InnerGraph::B() const {
  return boost::num_edges(_graph);
}

bool InnerGraph::identicalGraph(const InnerGraph& other) const {
  assert(N() == other.N() && B() == other.B());

  // Make sure topology matches
  return temple::all_of(
    boost::make_iterator_range(edges()),
    [&](const Edge& edge) -> bool {
      Edge correspondingEdge;
      bool edgeExists;
      std::tie(correspondingEdge, edgeExists) = boost::edge(
        boost::source(edge, _graph),
        boost::target(edge, _graph),
        other._graph
      );

      return edgeExists;
    }
  );
}

bool InnerGraph::unchangedSinceNotification() const {
  return _unchangedSinceNotification;
}

InnerGraph::VertexRange InnerGraph::vertices() const {
  return boost::vertices(_graph);
}

InnerGraph::EdgeRange InnerGraph::edges() const {
  return boost::edges(_graph);
}

InnerGraph::AdjacentVertexRange InnerGraph::adjacents(const Vertex a) const {
  return boost::adjacent_vertices(a, _graph);
}

InnerGraph::IncidentEdgeRange InnerGraph::edges(const Vertex a) const {
  return boost::out_edges(a, _graph);
}

const InnerGraph::BGLType& InnerGraph::bgl() const {
  return _graph;
}

InnerGraph::RemovalSafetyData InnerGraph::_removalSafetyData() const {
  RemovalSafetyData safetyData;

  std::vector<InnerGraph::Vertex> articulationVertices;

  using ComponentMapBase = std::map<InnerGraph::Edge, std::size_t>;

  ComponentMapBase componentMapData;
  boost::associative_property_map<ComponentMapBase> componentMap(componentMapData);
  std::size_t numComponents;

  // Calculate the biconnected components and articulation vertices
  std::tie(numComponents, std::ignore) = boost::biconnected_components(
    _graph,
    componentMap,
    std::back_inserter(articulationVertices)
  );

  // Copy articulation vertices to the set
  for(const auto& vertex : articulationVertices) {
    safetyData.articulationVertices.insert(vertex);
  }

  /* Work out from the biconnected components which edges are bridges: If the
   * biconnected component encompasses only a single edge, it is a bridge
   */
  std::vector<
    std::set<InnerGraph::Edge>
  > componentSets (numComponents);

  for(const auto& mapIterPair : componentMapData) {
    const auto& edgeIndex = mapIterPair.first;
    const auto& componentIndex = mapIterPair.second;

    componentSets[componentIndex].insert(edgeIndex);
  }

  for(const auto& componentSet : componentSets) {
    if(componentSet.size() == 1) {
      safetyData.bridges.insert(
        *componentSet.begin()
      );
    }
  }

  return safetyData;
}

} // namespace molassembler

} // namespace Scine
