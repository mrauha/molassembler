// Copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
// See LICENSE.txt for details.

#ifndef INCLUDE_MOLASSEMBLER_ATOM_stereopermutator_H
#define INCLUDE_MOLASSEMBLER_ATOM_stereopermutator_H

#include "molassembler/Options.h"
#include "molassembler/OuterGraph.h"

#if __cpp_lib_experimental_propagate_const >= 201505
#define MOLASSEMBLER_ENABLE_PROPAGATE_CONST
#include <experimental/propagate_const>
#endif

using namespace std::string_literals;

/*! @file
 *
 * @brief Handle arrangements of substituents around an atom-centered symmetry
 *
 * Coordinative stereopermutator class header file. Permits the storage of
 * particular arrangements of bonded atoms around a central atom and their
 * manipulation.
 *
 * Handles the stereopermutation issue, allowing users to cycle through
 * non-mutually-superimposable arrangements of substituents, here called
 * 'assignments'.
 */

/* Forward declarations */

namespace molassembler {

struct RankingInformation;

namespace DistanceGeometry {

class SpatialModel;
struct ChiralityConstraint;

} // namespace DistanceGeometry

} // namespace molassembler


namespace molassembler {

class AtomStereopermutator {
public:
//!@name Special member functions
//!@{
  AtomStereopermutator(AtomStereopermutator&& other) noexcept;
  AtomStereopermutator& operator = (AtomStereopermutator&& other) noexcept;
  AtomStereopermutator(const AtomStereopermutator& other);
  AtomStereopermutator& operator = (const AtomStereopermutator& other);
  ~AtomStereopermutator();

  /*!
   * @brief Construct an AtomStereopermutator
   *
   * @param graph The molecule's graph. This information is needed to model
   *   haptic ligands.
   * @param symmetry The local idealized symmetry to model. Typically the
   *   result of Molecule's determineLocalSymmetry.
   * @param centerAtom The atom index within the molecule that is the center of
   *   the local idealized symmetry
   * @param ranking The ranking of the central atom's substituents and ligand
   *   sites. Typically the result of Molecule's rankPriority.
   */
  AtomStereopermutator(
    // The base graph
    const OuterGraph& graph,
    // The symmetry of this Stereopermutator
    Symmetry::Name symmetry,
    // The atom this Stereopermutator is centered on
    AtomIndex centerAtom,
    // Ranking information of substituents
    RankingInformation ranking
  );
//!@}

//!@name Modifiers
//!@{
  /*!
   * @brief Add a new substituent to the permutator, propagating chiral state
   *
   * Handles the addition of a new substituent to the stereopermutator. If the
   * stereopermutator contains chiral state, it is attempted to transfer the state
   * into the new assignment space according to the supplied chiral state
   * preservation options
   *
   * @param graph The molecule's graph which this permutator helps model.
   * @param newSubstituentIndex The atom index of the new substituent
   * @param newRanking The updated ranking information (after graph addition of
   *   the new substituent)
   * @param newSymmetry The target symmetry of increased size
   * @param preservationOption The behavioral option deciding how chiral state
   *   is propagated.
   */
  void addSubstituent(
    const OuterGraph& graph,
    AtomIndex newSubstituentIndex,
    RankingInformation newRanking,
    Symmetry::Name newSymmetry,
    ChiralStatePreservation preservationOption
  );

  //! Changes the assignment of the stereopermutator
  void assign(boost::optional<unsigned> assignment);

  /*!
   * @brief Assign the Stereopermutator randomly using relative statistical weights
   *
   * Stereopermutations are generated with relative statistical occurrence
   * weights. The assignment is then chosen from the possible stereopermutations
   * with a discrete distribution whose weights are the corresponding relative
   * statistical occurrences.
   *
   * @note If the stereocenter is already assigned, it is reassigned.
   */
  void assignRandom();

  /*!
   * @brief Determine the symmetry and assignment realized in positions
   *
   * The symmetry and assignment are determined based on three-dimensional
   * positions using angle and chiral distortions from the respective idealized
   * symmetries.
   *
   * @param graph The molecule's graph which this permutator helps model
   * @param angstromWrapper The wrapped positions
   * @param excludeSymmetries Any symmetries that should be excluded from
   *   the fitting procedure
   *
   * @note Distorted tetrahedral structures are often closer to seesaw than
   *   tetrahedral. It is advisable to bias fitting towards tetrahedral (by
   *   exclusion) in cases where seesaw is not expected.
   */
  void fit(
    const OuterGraph& graph,
    const AngstromWrapper& angstromWrapper,
    const std::vector<Symmetry::Name>& excludeSymmetries = {}
  );

  /*!
   * @brief Propagate the stereocenter state through a possible ranking change
   *
   * In case a graph modification changes the ranking of this stereopermutator's
   * substituents, it must be redetermined whether the new configuration is a
   * stereopermutator and if so, which assignment corresponds to the previous one.
   */
  void propagateGraphChange(
    const OuterGraph& graph,
    RankingInformation newRanking
  );

  /*!
   * @brief Adapts atom indices in the internal state to the removal of an atom
   *
   * Atom indices are adapted to a graph-level removal of an atom. The removed
   * index is changed to a placeholder value.
   */
  void propagateVertexRemoval(AtomIndex removedIndex);

  /*!
   * @brief Removes a substituent, propagating state to the new smaller symmetry
   *
   * Handles the removal of a substituent from the stereopermutator. If the
   * stereopermutator carries chiral information, a new assignment can be chosen
   * according to the supplide chiral state preservation option.
   *
   * @warning This should be called after the removal of an atom on the graph level
   */
  void removeSubstituent(
    const OuterGraph& graph,
    AtomIndex which,
    RankingInformation newRanking,
    Symmetry::Name newSymmetry,
    ChiralStatePreservation preservationOption
  );

  /*!
   * @brief Change the symmetry of the permutator
   *
   * @post The permutator is unassigned (chiral state is discarded)
   */
  void setSymmetry(
    Symmetry::Name symmetryName,
    const OuterGraph& graph
  );
//!@}

//!@name Information
//!@{
  /*!
   * @brief Fetches angle between substituent ligands in the idealized symmetry
   *
   * @param i Ligand index one
   * @param j Ligand index two
   *
   * @pre @p i and @p j are valid ligand indices into the underlying
   * RankingInformation's RankingInformation#ligands member.
   *
   * @sa getRanking()
   */
  double angle(unsigned i, unsigned j) const;

  /*! Returns the permutation index within the set of possible permutations, if set
   *
   * Returns the (public) information of whether the stereopermutator is assigned
   * or not, and if so, which assignment it is.
   */
  boost::optional<unsigned> assigned() const;

  //! Returns a single-element vector containing the central atom
  AtomIndex centralIndex() const;

  /*! Returns IOP within the set of symbolic ligand permutations
   *
   * This is different to the assignment. The assignment denotes the index
   * within the set of possible (more specifically, not obviously infeasible)
   * stereopermutations.
   */
  boost::optional<unsigned> indexOfPermutation() const;

  /*! Returns a minimal representation of chirality constraints
   *
   * Every minimal representation consists only of ligand indices.
   *
   * The minimal representation assumes that all Symmetry tetrahedron
   * definitions are defined to be Positive targets, which is checked in
   * the chemical_symmetries tests.
   */
  std::vector<
    std::array<boost::optional<unsigned>, 4>
  > minimalChiralityConstraints() const;

  //! Generates a list of chirality constraints on its substituents for DG
  std::vector<DistanceGeometry::ChiralityConstraint> chiralityConstraints(
    double looseningMultiplier
  ) const;

  //! Returns an information string for diagnostic purposes
  std::string info() const;

  //! Returns an information string for ranking equality checking purposes
  std::string rankInfo() const;

  //! Returns the underlying ranking
  const RankingInformation& getRanking() const;

  //! Returns the underlying symmetry
  Symmetry::Name getSymmetry() const;

  /*! Yields the mapping from ligand indices to symmetry positions
   *
   * \throws std::logic_error if the stereopermutator is unassigned.
   */
  std::vector<unsigned> getSymmetryPositionMap() const;

  /*! Returns the number of possible assignments
   *
   * The number of possible assignments is the number of non-superposable
   * arrangements of the abstract ligand case reduced by trans-arranged
   * multidentate pairs where the bridge length is too short or overlapping
   * haptic cones.
   *
   * For instance, if octahedral M[(A-A)3], there are four abstract arrangements
   * - trans-trans-trans
   * - trans-cis-cis
   * - 2x cis-cis-cis (Δ and Λ isomers, ship propeller-like chirality)
   *
   * However, the number of stereopermutations for a concrete case in which the
   * bridges are too short to allow trans bonding is reduced by all
   * arrangements containing a trans-bonded bidentate ligand, i.e. only Δ and Λ
   * remain. The number of assignments is then only two.
   *
   * This is the upper exclusive bound on Some-type arguments to assign().
   */
  unsigned numAssignments() const;

  /*! Returns the number of possible stereopermutations
   *
   * The number of possible stereopermutations is the number of
   * non-superposable arrangements of the abstract ligand case without removing
   * trans-arranged multidentate pairs or overlapping haptic cones.
   *
   * For instance, if octahedral M[(A-A)3], there are four abstract arrangements
   * - trans-trans-trans
   * - trans-cis-cis
   * - 2x cis-cis-cis (Δ and Λ isomers, ship propeller-like chirality)
   *
   * However, the number of assignments for a concrete case in which the bridges
   * are too short to allow trans binding is reduced by all arrangements
   * containing a trans-bonded bidentate ligand, i.e. only Δ and Λ remain.
   *
   * Fetches the number of permutations determined by symbolic ligand
   * calculation, not considering linking or haptic ligand cones.
   */
  unsigned numStereopermutations() const;

  void setModelInformation(
    DistanceGeometry::SpatialModel& model,
    const std::function<double(const AtomIndex)>& cycleMultiplierForIndex,
    double looseningMultiplier
  ) const;
//!@}


//!@name Operators
//!@{
  bool operator == (const AtomStereopermutator& other) const;
  bool operator != (const AtomStereopermutator& other) const;
  bool operator < (const AtomStereopermutator& other) const;
//!@}

private:
  class Impl;

#ifdef MOLASSEMBLER_ENABLE_PROPAGATE_CONST
  std::experimental::propagate_const<
    std::unique_ptr<Impl>
  > _pImpl;
#else
  std::unique_ptr<Impl> _pImpl;
#endif
};

} // namespace molassembler

#endif
