/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief Directed conformer generation class and helper functions implementation file
 */

#ifndef INCLUDE_MOLASSEMBLER_DIRECTED_CONFORMER_GENERATOR_IMPL_H
#define INCLUDE_MOLASSEMBLER_DIRECTED_CONFORMER_GENERATOR_IMPL_H

#include "molassembler/DirectedConformerGenerator.h"
#include "molassembler/Molecule.h"

#include "temple/BoundedNodeTrie.h"

namespace Scine {
namespace molassembler {

class DirectedConformerGenerator::Impl {
public:
  static unsigned distance(
    const DecisionList& a,
    const DecisionList& b,
    const DecisionList& bounds
  );

  static boost::variant<IgnoreReason, BondStereopermutator> considerBond(
    const BondIndex& bondIndex,
    const Molecule& molecule,
    const std::map<AtomIndex, unsigned>& smallestCycleMap
  );

  Impl(Molecule molecule);

  DecisionList generateNewDecisionList();

  bool insert(const DecisionList& decisionList) {
    return _decisionLists.insert(decisionList);
  }

  bool contains(const DecisionList& decisionList) {
    return _decisionLists.contains(decisionList);
  }

  const BondList& bondList() const {
    return _relevantBonds;
  }

  unsigned conformerCount() const {
    return _decisionLists.size();
  }

  unsigned idealEnsembleSize() const {
    return _decisionLists.capacity();
  }

  outcome::result<Utils::PositionCollection> generateConformation(
    const DecisionList& decisionList,
    const DistanceGeometry::Configuration& configuration
  );

  DecisionList getDecisionList(Utils::PositionCollection positions) const;

private:
  Molecule _molecule;
  BondList _relevantBonds;

  /* This data structure is primarily designed for use as a set-like type that
   * can contain all choices of discrete enumerated dihedral positions for a
   * dihedral chain.
   *
   * Say you have three different dihedrals, each of which may have a distinct
   * number of possible rotations (although in organic molecules, the most
   * common one will naturally be three). Then the bounds for construction of
   * this tree are e.g. {3, 2, 4}.
   *
   * Possible Decisionlists are then e.g.:
   * - {0, 0, 0}, (this is the minimal choice list if ordered lexicographically)
   * - {2, 1, 3}, (this is the maximal choice list if ordered lexicographically)
   * - {1, 1, 3},
   * - ...
   *
   * This data structure can help you keep track of which choices at each
   * dihedral you have explored and which ones might lead to a conformer that
   * is most different from the ones you already have.
   */
  temple::BoundedNodeTrie<std::uint8_t> _decisionLists;
};

} // namespace molassembler
} // namespace Scine

#endif