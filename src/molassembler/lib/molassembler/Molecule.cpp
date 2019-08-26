/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#include "molassembler/Molecule/MoleculeImpl.h"

namespace Scine {

namespace molassembler {

/* Molecule interface to Impl call forwards */
Molecule::Molecule() noexcept : _pImpl(
  std::make_unique<Impl>()
) {}

Molecule::Molecule(Molecule&& other) noexcept = default;
Molecule& Molecule::operator = (Molecule&& rhs) noexcept = default;

Molecule::Molecule(const Molecule& other) : _pImpl(
  std::make_unique<Impl>(*other._pImpl)
) {}
Molecule& Molecule::operator = (const Molecule& rhs) {
  *_pImpl = *rhs._pImpl;
  return *this;
}

Molecule::~Molecule() = default;

Molecule::Molecule(const Utils::ElementType element) noexcept : _pImpl(
  std::make_unique<Impl>(element)
) {}

Molecule::Molecule(
  const Utils::ElementType a,
  const Utils::ElementType b,
  const BondType bondType
) noexcept : _pImpl(
  std::make_unique<Impl>(a, b, bondType)
) {}

Molecule::Molecule(OuterGraph graph) : _pImpl(
  std::make_unique<Impl>(std::move(graph))
) {}

Molecule::Molecule(
  OuterGraph graph,
  const AngstromWrapper& positions,
  const boost::optional<
    std::vector<BondIndex>
  >& bondStereopermutatorCandidatesOptional
) : _pImpl(
  std::make_unique<Impl>(
    std::move(graph),
    positions,
    bondStereopermutatorCandidatesOptional
  )
) {}

Molecule::Molecule(
  OuterGraph graph,
  StereopermutatorList stereopermutators,
  const AtomEnvironmentComponents canonicalComponents
) : _pImpl(
  std::make_unique<Impl>(
    std::move(graph),
    std::move(stereopermutators),
    canonicalComponents
  )
) {}

/* Modifiers */
AtomIndex Molecule::addAtom(
  const Utils::ElementType elementType,
  const AtomIndex adjacentTo,
  const BondType bondType
) {
  return _pImpl->addAtom(elementType, adjacentTo, bondType);
}

BondIndex Molecule::addBond(
  const AtomIndex a,
  const AtomIndex b,
  const BondType bondType
) {
  return _pImpl->addBond(a, b, bondType);
}

void Molecule::applyPermutation(const std::vector<AtomIndex>& permutation) {
  _pImpl->applyPermutation(permutation);
}

void Molecule::assignStereopermutator(
  const AtomIndex a,
  const boost::optional<unsigned>& assignment
) {
  _pImpl->assignStereopermutator(a, assignment);
}

void Molecule::assignStereopermutator(
  const BondIndex& edge,
  const boost::optional<unsigned>& assignment
) {
  _pImpl->assignStereopermutator(edge, assignment);
}

void Molecule::assignStereopermutatorRandomly(const AtomIndex a) {
  _pImpl->assignStereopermutatorRandomly(a);
}

void Molecule::assignStereopermutatorRandomly(const BondIndex& e) {
  _pImpl->assignStereopermutatorRandomly(e);
}

std::vector<AtomIndex> Molecule::canonicalize(
  const AtomEnvironmentComponents componentBitmask
) {
  return _pImpl->canonicalize(componentBitmask);
}

void Molecule::removeAtom(const AtomIndex a) {
  _pImpl->removeAtom(a);
}

void Molecule::removeBond(
  const AtomIndex a,
  const AtomIndex b
) {
  _pImpl->removeBond(a, b);
}

void Molecule::removeBond(
  const BondIndex& bond
) {
  _pImpl->removeBond(bond.first, bond.second);
}

bool Molecule::setBondType(
  const AtomIndex a,
  const AtomIndex b,
  const BondType bondType
) {
  return _pImpl->setBondType(a, b, bondType);
}

void Molecule::setElementType(
  const AtomIndex a,
  const Utils::ElementType elementType
) {
  _pImpl->setElementType(a, elementType);
}

void Molecule::setGeometryAtAtom(
  const AtomIndex a,
  const Symmetry::Name symmetryName
) {
  _pImpl->setGeometryAtAtom(a, symmetryName);
}


/* Information */
AtomEnvironmentComponents Molecule::canonicalComponents() const {
  return _pImpl->canonicalComponents();
}

boost::optional<Symmetry::Name> Molecule::inferSymmetry(
  const AtomIndex index,
  const RankingInformation& ranking
) const {
  return _pImpl->inferSymmetry(index, ranking);
}

std::string Molecule::dumpGraphviz() const {
  return _pImpl->dumpGraphviz();
}

const OuterGraph& Molecule::graph() const {
  return _pImpl->graph();
}

const StereopermutatorList& Molecule::stereopermutators() const {
  return _pImpl->stereopermutators();
}

StereopermutatorList Molecule::inferStereopermutatorsFromPositions(
  const AngstromWrapper& angstromWrapper,
  const boost::optional<
    std::vector<BondIndex>
  >& explicitBondStereopermutatorCandidatesOption
) const {
  return _pImpl->inferStereopermutatorsFromPositions(
    angstromWrapper,
    explicitBondStereopermutatorCandidatesOption
  );
}

bool Molecule::canonicalCompare(
  const Molecule& other,
  const AtomEnvironmentComponents componentBitmask
) const {
  return _pImpl->canonicalCompare(*other._pImpl, componentBitmask);
}

bool Molecule::modularCompare(
  const Molecule& other,
  const AtomEnvironmentComponents componentBitmask
) const {
  return _pImpl->modularCompare(*other._pImpl, componentBitmask);
}

RankingInformation Molecule::rankPriority(
  const AtomIndex a,
  const std::vector<AtomIndex>& excludeAdjacent,
  const boost::optional<AngstromWrapper>& positionsOption
) const {
  return _pImpl->rankPriority(a, excludeAdjacent, positionsOption);
}

/* Operators */
bool Molecule::operator == (const Molecule& other) const {
  return *_pImpl == *other._pImpl;
}

bool Molecule::operator != (const Molecule& other) const {
  return *_pImpl != *other._pImpl;
}


} // namespace molassembler

} // namespace Scine

std::ostream& operator << (
  std::ostream& os,
  const Scine::molassembler::Molecule& molecule
) {
  const auto& stereopermutators = molecule.stereopermutators();

  if(!stereopermutators.empty()) {
    os << "Stereopermutator information:\n";

    for(const auto& stereopermutator : stereopermutators.atomStereopermutators()) {
      os << stereopermutator.info() << "\n";
    }

    for(const auto& stereopermutator : stereopermutators.bondStereopermutators()) {
      os << stereopermutator.info() << "\n";
    }
  }

  return os;
}
