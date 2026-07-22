//===- GraphTraitsModels.h -- minimal GenericGraphTraits capability checks -===//
//
//                     SVF: Static Value-Flow Analysis
//
// Copyright (C) <2013->  <Yulei Sui>
//


// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.

// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//

/*
 * GraphTraitsModels.h
 *
 *      Author: Jiawei Yang
 *
 * C++17 detection-idiom capability checks for GenericGraphTraits<G>. These say
 * "G's traits provide AT LEAST these operations" (extra members are fine), so
 * use sites (e.g. GraphWriter) can static_assert a clear requirement instead of
 * failing deep inside a template. They work whether the traits' node reference
 * is a pointer (classic graphs) or a value (sliced views), by deriving the node
 * reference from the nodes iterator's value_type.
 */

#ifndef GRAPHS_GRAPHTRAITSMODELS_H
#define GRAPHS_GRAPHTRAITSMODELS_H

#include "Graphs/GenericGraph.h"   // GenericGraphTraits
#include "Graphs/GraphTraits.h"    // Inverse
#include <iterator>
#include <type_traits>
#include <utility>

namespace SVF
{

// The node reference a graph's traits traffic in, derived from its nodes iterator.
template <typename G>
using traits_node_ref =
    typename std::iterator_traits<decltype(
        GenericGraphTraits<G>::nodes_begin(std::declval<G>()))>::value_type;

//===----------------------------------------------------------------------===//
// Individual capability probes.
//===----------------------------------------------------------------------===//
template <typename G, typename = void>
struct has_node_iteration : std::false_type {};
template <typename G>
struct has_node_iteration<G, std::void_t<
        decltype(GenericGraphTraits<G>::nodes_begin(std::declval<G>())),
        decltype(GenericGraphTraits<G>::nodes_end(std::declval<G>()))>>
    : std::true_type {};

template <typename G, typename = void>
struct has_child_iteration : std::false_type {};
template <typename G>
struct has_child_iteration<G, std::void_t<
        decltype(GenericGraphTraits<G>::child_begin(std::declval<traits_node_ref<G>>())),
        decltype(GenericGraphTraits<G>::child_end(std::declval<traits_node_ref<G>>()))>>
    : std::true_type {};

// Reverse child iteration through the Inverse<> traits, over the same node ref.
template <typename G, typename = void>
struct has_inverse_child_iteration : std::false_type {};
template <typename G>
struct has_inverse_child_iteration<G, std::void_t<
        decltype(GenericGraphTraits<Inverse<G>>::child_begin(std::declval<traits_node_ref<G>>())),
        decltype(GenericGraphTraits<Inverse<G>>::child_end(std::declval<traits_node_ref<G>>()))>>
    : std::true_type {};

template <typename G, typename = void>
struct has_edge_iteration : std::false_type {};
template <typename G>
struct has_edge_iteration<G, std::void_t<
        decltype(GenericGraphTraits<G>::child_edge_begin(std::declval<traits_node_ref<G>>())),
        decltype(GenericGraphTraits<G>::child_edge_end(std::declval<traits_node_ref<G>>())),
        decltype(GenericGraphTraits<G>::edge_dest(
                     std::declval<typename GenericGraphTraits<G>::EdgeRef>()))>>
    : std::true_type {};

template <typename G, typename = void>
struct has_indexing : std::false_type {};
template <typename G>
struct has_indexing<G, std::void_t<
        decltype(GenericGraphTraits<G>::getNodeID(std::declval<traits_node_ref<G>>())),
        decltype(GenericGraphTraits<G>::getNode(std::declval<G>(), std::declval<NodeID>())),
        decltype(GenericGraphTraits<G>::direct_child_begin(std::declval<traits_node_ref<G>>()))>>
    : std::true_type {};

//===----------------------------------------------------------------------===//
// Composed capability models.
//===----------------------------------------------------------------------===//
// Can enumerate nodes and each node's successors (what GraphWriter needs).
template <typename G>
struct GraphTraversalModel
    : std::integral_constant<bool,
      has_node_iteration<G>::value && has_child_iteration<G>::value> {};

// Forward and reverse traversal.
template <typename G>
struct BidirectionalGraphModel
    : std::integral_constant<bool,
      GraphTraversalModel<G>::value && has_inverse_child_iteration<G>::value> {};

// First-class edges (edge iteration + edge_dest) carrying domain info.
template <typename G>
struct EdgeAwareGraphModel
    : std::integral_constant<bool, has_edge_iteration<G>::value> {};

// Stable node ids and id->node lookup (+ direct children for SCC).
template <typename G>
struct IndexedGraphModel
    : std::integral_constant<bool, has_indexing<G>::value> {};

} // End namespace SVF

#endif // GRAPHS_GRAPHTRAITSMODELS_H
