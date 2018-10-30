#include "molassembler/Stereopermutators/AtomStereopermutatorImpl.h"

#include "chemical_symmetries/Properties.h"
#include "chemical_symmetries/DynamicProperties.h"
#include "CyclicPolygons.h"
#include <Eigen/Dense>
#include "stereopermutation/GenerateUniques.h"
#include "temple/Adaptors/AllPairs.h"
#include "temple/Adaptors/Iota.h"
#include "temple/Adaptors/Transform.h"
#include "temple/Functional.h"
#include "temple/constexpr/Numeric.h"
#include "temple/Optionals.h"
#include "temple/Random.h"

#include "molassembler/Cycles.h"
#include "molassembler/Detail/BuildTypeSwitch.h"
#include "molassembler/Detail/DelibHelpers.h"
#include "molassembler/Detail/StdlibTypeAlgorithms.h"
#include "molassembler/DistanceGeometry/SpatialModel.h"
#include "molassembler/DistanceGeometry/ValueBounds.h"
#include "molassembler/Graph/InnerGraph.h"
#include "molassembler/Log.h"
#include "molassembler/Modeling/CommonTrig.h"

namespace molassembler {

/* Static functions */
/* Constructors */
AtomStereopermutator::Impl::Impl(
  const OuterGraph& graph,
  // The symmetry of this Stereopermutator
  const Symmetry::Name symmetry,
  // The atom this Stereopermutator is centered on
  const AtomIndex centerAtom,
  // Ranking information of substituents
  RankingInformation ranking
) : _ranking {std::move(ranking)},
    _centerAtom {centerAtom},
    _symmetry {symmetry},
    _assignmentOption {boost::none}
{
  _cache = PermutationState {
    _ranking,
    _centerAtom,
    _symmetry,
    graph
  };
}

/* Modification */
void AtomStereopermutator::Impl::addSubstituent(
  const OuterGraph& graph,
  const AtomIndex newSubstituentIndex,
  RankingInformation newRanking,
  const Symmetry::Name newSymmetry,
  const ChiralStatePreservation preservationOption
) {
  // Calculate set of new permutations from changed parameters
  PermutationState newPermutationState {
    newRanking,
    _centerAtom,
    newSymmetry,
    graph
  };

  /* Try to find a continuation of chiral state (index of permutation in the new
   * set of permutations)
   */
  boost::optional<unsigned> newStereopermutation = boost::none;

  /* Two possible situations: Either a full ligand is added, or an atom is
   * added to a ligand
   */
  boost::optional<bool> soleConstitutingIndex;
  unsigned ligandIndexAddedTo;
  for(
    unsigned ligandI = 0;
    ligandI < newRanking.ligands.size() && !soleConstitutingIndex;
    ++ligandI
  ) {
    for(const AtomIndex constitutingIndex : newRanking.ligands.at(ligandI)) {
      if(constitutingIndex == newSubstituentIndex) {
        ligandIndexAddedTo = ligandI;
        soleConstitutingIndex = (newRanking.ligands.at(ligandI).size() == 1);
        break;
      }
    }
  }

  // No need to find a new assignment if no chiral state is present
  if(_assignmentOption && numStereopermutations() > 1) {
    // Transfer indices from smaller symmetry to larger
    std::vector<unsigned> ligandsAtNewSymmetryPositions;

    if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry)) {
      /* If no symmetry transition happens, then all we have to figure out is a
       * ligand to ligand mapping (since ligands may have reordered completely)
       */
      assert(!soleConstitutingIndex.value());

      // Sort ligands in both rankings so we can use lexicographical comparison
      _ranking.ligands.at(ligandIndexAddedTo).push_back(newSubstituentIndex);
      for(auto& ligand : _ranking.ligands) {
        temple::inplace::sort(ligand);
      }

      for(auto& ligand : newRanking.ligands) {
        temple::inplace::sort(ligand);
      }

      auto ligandMapping = temple::map(
        _ranking.ligands,
        [&newRanking](const auto& ligand) -> unsigned {
          auto findIter = std::find(
            newRanking.ligands.begin(),
            newRanking.ligands.end(),
            ligand
          );

          assert(findIter != newRanking.ligands.end());

          return findIter - newRanking.ligands.begin();
        }
      );

      ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));
      for(unsigned i = 0; i < ligandMapping.size(); ++i) {
        ligandsAtNewSymmetryPositions.at(i) = ligandMapping.at(
          _cache.symmetryPositionMap.at(i)
        );
      }
    } else if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry) + 1) {
      assert(soleConstitutingIndex.value());

      /* Try to get a mapping to the new symmetry
       * If that returns a Some, try to get a mapping by preservationOption policy
       *
       * If any of these steps returns boost::none, the whole expression is
       * boost::none.
       */
      auto suitableMappingOption = Symmetry::getMapping(
        _symmetry,
        newSymmetry,
        boost::none
      ) | temple::callIfSome(
        PermutationState::getIndexMapping,
        temple::ANS,
        preservationOption
      );

      if(suitableMappingOption) {
        /* So now we must transfer the current assignment into the new symmetry
         * and search for it in the set of uniques.
         */
        const auto& symmetryMapping = suitableMappingOption.value();


        // Copy over the current symmetry position map
        std::vector<unsigned> ligandsAtOldSymmetryPositions = _cache.symmetryPositionMap;
        ligandsAtOldSymmetryPositions.push_back(ligandIndexAddedTo);
        ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));

        for(unsigned i = 0; i < Symmetry::size(newSymmetry); ++i) {
          ligandsAtNewSymmetryPositions.at(
            symmetryMapping.at(i)
          ) = ligandsAtOldSymmetryPositions.at(i);
        }
      }
      /* If no mapping can be found that fits to the preservationOption,
       * newStereopermutation remains boost::none, and this stereopermutator loses
       * any chiral information it may have had.
       */
    }

    if(!ligandsAtNewSymmetryPositions.empty()) {
      // Get character representation in new symmetry
      std::vector<char> charactersInNewSymmetry = PermutationState::makeStereopermutationCharacters(
        newPermutationState.canonicalLigands,
        newPermutationState.symbolicCharacters,
        ligandsAtNewSymmetryPositions
      );

      // Construct an assignment from it
      auto trialStereopermutation = stereopermutation::Stereopermutation(
        newSymmetry,
        charactersInNewSymmetry,
        newPermutationState.selfReferentialLinks
      );

      // Generate the rotational equivalents
      auto allTrialRotations = trialStereopermutation.generateAllRotations(newSymmetry);

      // Search for a match from the vector of uniques
      for(unsigned i = 0; i < newPermutationState.permutations.assignments.size(); ++i) {
        if(allTrialRotations.count(newPermutationState.permutations.assignments.at(i)) > 0) {
          newStereopermutation = i;
          break;
        }
      }
    }
  }

  // Overwrite class state
  _ranking = std::move(newRanking);
  _symmetry = newSymmetry;
  _cache = std::move(newPermutationState);

  assign(newStereopermutation);
}

void AtomStereopermutator::Impl::assign(boost::optional<unsigned> assignment) {
  if(assignment) {
    assert(assignment.value() < _cache.feasiblePermutations.size());
  }

  // Store current assignment
  _assignmentOption = std::move(assignment);

  /* save a mapping of next neighbor indices to symmetry positions after
   * assigning (AtomIndex -> unsigned).
   */
  if(_assignmentOption) {
    _cache.symmetryPositionMap = PermutationState::generateLigandToSymmetryPositionMap(
      _cache.permutations.assignments.at(
        _cache.feasiblePermutations.at(
          _assignmentOption.value()
        )
      ),
      _cache.canonicalLigands
    );
  } else { // Wipe the map
    _cache.symmetryPositionMap.clear();
  }
}

void AtomStereopermutator::Impl::assignRandom() {
  assign(
    temple::random::pickDiscrete(
      // Map the feasible permutations onto their weights
      temple::map(
        _cache.feasiblePermutations,
        [&](const unsigned permutationIndex) -> unsigned {
          return _cache.permutations.weights.at(permutationIndex);
        }
      ),
      randomnessEngine()
    )
  );
}

void AtomStereopermutator::Impl::propagateGraphChange(
  const OuterGraph& graph,
  RankingInformation newRanking
) {
  if(
    newRanking.ligandsRanking == _ranking.ligandsRanking
    && newRanking.links == _ranking.links
  ) {
    return;
  }

  PermutationState newPermutationState {
    newRanking,
    _centerAtom,
    _symmetry,
    graph
  };

  boost::optional<unsigned> newStereopermutation = boost::none;

  /* Before we overwrite class state, we need to figure out which assignment
   * in the new set of assignments corresponds to the one we have now.
   * This is only necessary in the case that the stereopermutator is currently
   * assigned and only possible if the new number of assignments is smaller or
   * equal to the amount we have currently.
   *
   * Additionally, in some circumstances, propagateGraphChange can be called
   * with either fewer or more ligands than the current ranking indicates. This
   * happens if e.g. a bond is added between ligands, forming a single haptic
   * ligand, or breaking a haptic ligand into two. These cases are excluded
   * with the condition of an equal number of ligands, and thus universally
   * lead to a loss of stereoinformation.
   */
  if(
    _assignmentOption
    && numStereopermutations() > 1
    && (
      newPermutationState.permutations.assignments.size()
      <= _cache.permutations.assignments.size()
    ) && newRanking.ligands.size() == _ranking.ligands.size()
  ) {
    const auto& currentStereopermutation = _cache.permutations.assignments.at(
      _cache.feasiblePermutations.at(
        _assignmentOption.value()
      )
    );

    // Replace the characters by their corresponding indices from the old ranking
    std::vector<unsigned> ligandsAtSymmetryPositions = PermutationState::generateSymmetryPositionToLigandMap(
      currentStereopermutation,
      _cache.canonicalLigands
    );

    // Replace the atom indices by their new ranking characters
    std::vector<char> newStereopermutationCharacters = PermutationState::makeStereopermutationCharacters(
      newPermutationState.canonicalLigands,
      newPermutationState.symbolicCharacters,
      ligandsAtSymmetryPositions
    );

    // Create a new assignment with those characters
    auto trialStereopermutation = stereopermutation::Stereopermutation(
      _symmetry,
      newStereopermutationCharacters,
      newPermutationState.selfReferentialLinks
    );

    // Generate all rotations of this trial assignment
    auto allTrialRotations = trialStereopermutation.generateAllRotations(_symmetry);

    // Find out which of the new assignments has a rotational equivalent
    for(unsigned i = 0; i < newPermutationState.permutations.assignments.size(); ++i) {
      if(allTrialRotations.count(newPermutationState.permutations.assignments.at(i)) > 0) {
        newStereopermutation = i;
        break;
      }
    }
  }

  // Overwrite the class state
  _ranking = std::move(newRanking);
  _cache = std::move(newPermutationState);
  assign(newStereopermutation);
}

void AtomStereopermutator::Impl::propagateVertexRemoval(const AtomIndex removedIndex) {
  /* This function replaces any occurrences of the atom index that is being
   * removed in the global state with a placeholder of the same type and updates
   * any invalidated atom indices.
   */

  /* If the central atom is being removed, just drop this stereopermutator
   * beforehand in caller. This would just be unnecessary work.
   */
  assert(_centerAtom != removedIndex);

  // Define some helper functions
  auto updateIndexInplace = [&removedIndex](AtomIndex& index) -> void {
    if(index > removedIndex) {
      --index;
    } else if(index == removedIndex) {
      index = InnerGraph::removalPlaceholder;
    }
  };

  auto updateIndex = [&removedIndex](const AtomIndex index) -> AtomIndex {
    if(index > removedIndex) {
      return index - 1;
    }

    if(index == removedIndex) {
      return InnerGraph::removalPlaceholder;
    }

    return index;
  };

  /* Update indices in RankingInformation */
  for(auto& equalPrioritySet : _ranking.sortedSubstituents) {
    for(auto& index : equalPrioritySet) {
      updateIndexInplace(index);
    }
  }

  for(auto& ligandIndicesList : _ranking.ligands) {
    for(auto& atomIndex : ligandIndicesList) {
      updateIndexInplace(atomIndex);
    }
  }

  for(auto& link : _ranking.links) {
    link.cycleSequence = temple::map(
      link.cycleSequence,
      updateIndex
    );
  }
}

void AtomStereopermutator::Impl::removeSubstituent(
  const OuterGraph& graph,
  const AtomIndex which,
  RankingInformation newRanking,
  const Symmetry::Name newSymmetry,
  const ChiralStatePreservation preservationOption
) {
  /* This function tries to find a new assignment for the situation in which
   * the previously replaced atom index is actually removed.
   *
   * Since the introduction of haptic ligands, the prior graph change can
   * encompass two things:
   * - A ligand that is comprised of a singular atom has been removed. The
   *   symmetry size is reduced and a state continuation must be found.
   * - A constituting atom of a haptic ligand has been removed. No symmetry
   *   change happens.
   */
  PermutationState newPermutationState {
    newRanking,
    _centerAtom,
    newSymmetry,
    graph
  };

  boost::optional<unsigned> newStereopermutation;

  /* Find out in which ligand the atom is removed, and whether it is the sole
   * constituting index
   */
  bool soleConstitutingIndex [[gnu::unused]];
  unsigned ligandIndexRemovedFrom;

  { // Temporary local scope to avoid pollution
    bool found = false;
    for(
      unsigned ligandI = 0;
      ligandI < _ranking.ligands.size() && !found;
      ++ligandI
    ) {
      for(const AtomIndex constitutingIndex : _ranking.ligands.at(ligandI)) {
        if(constitutingIndex == which) {
          found = true;
          soleConstitutingIndex = (_ranking.ligands.at(ligandI).size() == 1);
          ligandIndexRemovedFrom = ligandI;
        }
      }
    }

    if(!found) {
      throw std::logic_error("Ligand index being removed from not found!");
    }
  }

  // No need to find a new assignment if we currently do not carry chiral state
  if(_assignmentOption && numStereopermutations() > 1) {
    std::vector<unsigned> ligandsAtNewSymmetryPositions;

    if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry)) {
      /* If no symmetry transition happens, then all we have to figure out is a
       * ligand to ligand mapping.
       */
      assert(!soleConstitutingIndex);

      /* Sort ligands in the old ranking and new so we can use lexicographical
       * comparison to figure out a mapping
       */
      for(auto& ligand : _ranking.ligands) {
        temple::inplace::remove(ligand, which);
        temple::inplace::sort(ligand);
      }

      for(auto& ligand : newRanking.ligands) {
        temple::inplace::sort(ligand);
      }

      // Calculate the mapping from old ligands to new ones
      auto ligandMapping = temple::map(
        _ranking.ligands,
        [&newRanking](const auto& ligand) -> unsigned {
          auto findIter = std::find(
            newRanking.ligands.begin(),
            newRanking.ligands.end(),
            ligand
          );

          assert(findIter != newRanking.ligands.end());

          return findIter - newRanking.ligands.begin();
        }
      );

      ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));
      // Transfer ligands to new mapping
      for(unsigned i = 0; i < ligandMapping.size(); ++i) {
        ligandsAtNewSymmetryPositions.at(i) = ligandMapping.at(
          _cache.symmetryPositionMap.at(i)
        );
      }
    } else if(Symmetry::size(newSymmetry) == Symmetry::size(_symmetry) - 1) {
      assert(soleConstitutingIndex);
      /* Try to get a symmetry mapping to the new symmetry position
       * If there are mappings, try to select one according to preservationOption policy
       *
       * If any of those steps returns boost::none, the whole expression is
       * boost::none.
       */
      auto suitableMappingOptional = Symmetry::getMapping(
        _symmetry,
        newSymmetry,
        /* Last parameter is the deleted symmetry position, which is the
         * symmetry position at which the ligand being removed is currently at
         */
        _cache.symmetryPositionMap.at(ligandIndexRemovedFrom)
      ) | temple::callIfSome(
        PermutationState::getIndexMapping,
        temple::ANS,
        preservationOption
      );

      if(suitableMappingOptional) {
        const auto& symmetryMapping = suitableMappingOptional.value();

        // Transfer indices from current symmetry to new symmetry
        ligandsAtNewSymmetryPositions.resize(Symmetry::size(newSymmetry));
        for(unsigned i = 0; i < Symmetry::size(newSymmetry); ++i) {
          ligandsAtNewSymmetryPositions.at(i) = _cache.symmetryPositionMap.at(
            symmetryMapping.at(i)
          );
        }

        /* Now we have the old ligand indices in the new symmetry positions.
         * Since we know which ligand is deleted, we can decrement any indices
         * larger than it and obtain the new ligand indices.
         */
        for(auto& ligandIndex : ligandsAtNewSymmetryPositions) {
          if(ligandIndex > ligandIndexRemovedFrom) {
            --ligandIndex;
          }
        }
      }

      if(!ligandsAtNewSymmetryPositions.empty()) {
        // Get character representation in new symmetry
        std::vector<char> charactersInNewSymmetry = PermutationState::makeStereopermutationCharacters(
          newPermutationState.canonicalLigands,
          newPermutationState.symbolicCharacters,
          ligandsAtNewSymmetryPositions
        );

        //! @todo Shouldn't the links in the new symmetry be generated too for use in comparison??

        // Construct an assignment
        auto trialStereopermutation = stereopermutation::Stereopermutation(
          newSymmetry,
          charactersInNewSymmetry,
          newPermutationState.selfReferentialLinks
        );

        // Generate the rotational equivalents
        auto allTrialRotations = trialStereopermutation.generateAllRotations(newSymmetry);

        // Search for a match from the vector of uniques
        for(unsigned i = 0; i < newPermutationState.permutations.assignments.size(); ++i) {
          if(allTrialRotations.count(newPermutationState.permutations.assignments.at(i)) > 0) {
            newStereopermutation = i;
            break;
          }
        }
      }
    }
  }

  // Overwrite class state
  _ranking = std::move(newRanking);
  _symmetry = newSymmetry;
  _cache = std::move(newPermutationState);
  assign(newStereopermutation);
}

const PermutationState& AtomStereopermutator::Impl::getPermutationState() const {
  return _cache;
}

const RankingInformation& AtomStereopermutator::Impl::getRanking() const {
  return _ranking;
}

Symmetry::Name AtomStereopermutator::Impl::getSymmetry() const {
  return _symmetry;
}

std::vector<unsigned> AtomStereopermutator::Impl::getSymmetryPositionMap() const {
  if(_assignmentOption == boost::none) {
    throw std::logic_error(
      "The AtomStereopermutator is unassigned, ligands are not assigned to "
      "symmetry positions"
    );
  }

  return _cache.symmetryPositionMap;
}

void AtomStereopermutator::Impl::fit(
  const OuterGraph& graph,
  const AngstromWrapper& angstromWrapper,
  const std::vector<Symmetry::Name>& excludeSymmetries
) {
  // For all atoms making up a ligand, decide on the spatial average position
  const std::vector<Eigen::Vector3d> ligandPositions = temple::map(
    _ranking.ligands,
    [&angstromWrapper](const std::vector<AtomIndex>& ligandAtoms) -> Eigen::Vector3d {
      return DelibHelpers::averagePosition(angstromWrapper.positions, ligandAtoms);
    }
  );

  // Save stereopermutator state to return to if no fit is viable
  const Symmetry::Name priorSymmetry = _symmetry;
  const boost::optional<unsigned> priorStereopermutation  = _assignmentOption;

  const Symmetry::Name initialSymmetry {Symmetry::Name::Linear};
  const unsigned initialStereopermutation = 0;
  const double initialPenalty = 100;

  Symmetry::Name bestSymmetry = initialSymmetry;
  unsigned bestStereopermutation = initialStereopermutation;
  double bestPenalty = initialPenalty;
  unsigned bestStereopermutationMultiplicity = 1;

  auto excludesContains = temple::makeContainsPredicate(excludeSymmetries);

  // Cycle through all symmetries
  for(const auto& symmetryName : Symmetry::allNames) {
    // Skip any Symmetries of different size
    if(
      Symmetry::size(symmetryName) != Symmetry::size(_symmetry)
      || excludesContains(symmetryName)
    ) {
      continue;
    }

    // Change the symmetry of the AtomStereopermutator
    setSymmetry(symmetryName, graph);

    for(
      unsigned assignment = 0;
      assignment < numAssignments();
      ++assignment
    ) {
      // Assign the stereopermutator
      assign(assignment);

      const double angleDeviations = temple::sum(
        temple::adaptors::transform(
          temple::adaptors::allPairs(
            temple::adaptors::range(Symmetry::size(_symmetry))
          ),
          [&](const unsigned ligandI, const unsigned ligandJ) -> double {
            return std::fabs(
              DelibHelpers::angle(
                ligandPositions.at(ligandI),
                angstromWrapper.positions.at(_centerAtom).toEigenVector(),
                ligandPositions.at(ligandJ)
              ) - angle(ligandI, ligandJ)
            );
          }
        )
      );

      // We can stop immediately if this is worse
      if(angleDeviations > bestPenalty) {
        continue;
      }

      /*! @todo should this be kept at all? Just a follow-up error from the angle
       * What value does it bring?
       */
      const double oneThreeDistanceDeviations = temple::sum(
        temple::adaptors::transform(
          temple::adaptors::allPairs(
            temple::adaptors::range(Symmetry::size(_symmetry))
          ),
          [&](const unsigned ligandI, const unsigned ligandJ) -> double {
            return std::fabs(
              // ligandI - ligandJ 1-3 distance from positions
              DelibHelpers::distance(
                ligandPositions.at(ligandI),
                ligandPositions.at(ligandJ)
              )
              // idealized 1-3 distance from
              - CommonTrig::lawOfCosines(
                // i-j 1-2 distance from positions
                DelibHelpers::distance(
                  ligandPositions.at(ligandI),
                  angstromWrapper.positions.at(_centerAtom).toEigenVector()
                ),
                // j-k 1-2 distance from positions
                DelibHelpers::distance(
                  angstromWrapper.positions.at(_centerAtom).toEigenVector(),
                  ligandPositions.at(ligandJ)
                ),
                // idealized Stereopermutator angle
                angle(ligandI, ligandJ)
              )
            );
          }
        )
      );

      // Another early continue
      if(angleDeviations + oneThreeDistanceDeviations > bestPenalty) {
        continue;
      }

      const double chiralityDeviations = temple::sum(
        temple::adaptors::transform(
          minimalChiralityConstraints(),
          [&](const auto& minimalPrototype) -> double {
            auto fetchPosition = [&](const boost::optional<unsigned>& ligandIndexOptional) -> Eigen::Vector3d {
              if(ligandIndexOptional) {
                return ligandPositions.at(ligandIndexOptional.value());
              }

              return angstromWrapper.positions.at(_centerAtom).asEigenVector();
            };

            double volume = DelibHelpers::adjustedSignedVolume(
              fetchPosition(minimalPrototype[0]),
              fetchPosition(minimalPrototype[1]),
              fetchPosition(minimalPrototype[2]),
              fetchPosition(minimalPrototype[3])
            );

            // minimalChiralityConstraints() supplies only Positive targets
            if(volume < 0) {
              return 1;
            }

            return 0;
          }
        )
      );

      double fitPenalty = angleDeviations
        + oneThreeDistanceDeviations
        + chiralityDeviations;


#ifndef NDEBUG
      Log::log(Log::Particulars::AtomStereopermutatorFit)
        << Symmetry::nameIndex(symmetryName)
        << ", " << assignment
        << ", " << std::setprecision(4) << std::fixed
        << angleDeviations << ", "
        << oneThreeDistanceDeviations << ", "
        << chiralityDeviations
        << std::endl;
#endif

      if(fitPenalty < bestPenalty) {
        bestSymmetry = symmetryName;
        bestStereopermutation = assignment;
        bestPenalty = fitPenalty;
        bestStereopermutationMultiplicity = 1;
      } else if(fitPenalty == bestPenalty) {
        // Assume that IF we have multiplicity, it's from the same symmetry
        assert(bestSymmetry == symmetryName);
        bestStereopermutationMultiplicity += 1;
      }
    }
  }

  /* In case NO assignments could be tested, return to the prior state.
   * This guards against situations in which predicates in
   * uniques could lead no assignments to be returned, such as
   * in e.g. square-planar AAAB with {0, 3}, {1, 3}, {2, 3} with removal of
   * trans-spanning groups. In that situation, all possible assignments are
   * trans-spanning and uniques is an empty vector.
   *
   * At the moment, this predicate is disabled, so no such issues should arise.
   * Just being safe.
   */
  if(
    bestSymmetry == initialSymmetry
    && bestStereopermutation == initialStereopermutation
    && bestPenalty == initialPenalty
  ) {
    // Return to prior
    setSymmetry(priorSymmetry, graph);
    assign(priorStereopermutation);
  } else {
    // Set to best fit
    setSymmetry(bestSymmetry, graph);

    /* How to handle multiplicity?
     * Current policy: If there is multiplicity, do not assign
     */
    if(bestStereopermutationMultiplicity > 1) {
      assign(boost::none);
    } else {
      assign(bestStereopermutation);
    }
  }
}

/* Information */
double AtomStereopermutator::Impl::angle(
  const unsigned i,
  const unsigned j
) const {
  assert(i != j);
  assert(!_cache.symmetryPositionMap.empty());

  return Symmetry::angleFunction(_symmetry)(
    _cache.symmetryPositionMap.at(i),
    _cache.symmetryPositionMap.at(j)
  );
}

boost::optional<unsigned> AtomStereopermutator::Impl::assigned() const {
  return _assignmentOption;
}

AtomIndex AtomStereopermutator::Impl::centralIndex() const {
  return _centerAtom;
}

boost::optional<unsigned> AtomStereopermutator::Impl::indexOfPermutation() const {
  if(_assignmentOption) {
    return _cache.feasiblePermutations.at(_assignmentOption.value());
  }

  return boost::none;
}

std::vector<
  std::array<boost::optional<unsigned>, 4>
> AtomStereopermutator::Impl::minimalChiralityConstraints() const {
  std::vector<
    std::array<boost::optional<unsigned>, 4>
  > precursors;

  // Only collect constraints if it's actually assigned
  if(_assignmentOption && numStereopermutations() > 1) {

    /* Invert _neighborSymmetryPositionMap, we need a mapping of
     *  (position in symmetry) -> atom index
     */
    auto symmetryPositionToLigandIndexMap = PermutationState::generateSymmetryPositionToLigandMap(
      _cache.permutations.assignments.at(
        _cache.feasiblePermutations.at(
          _assignmentOption.value()
        )
      ),
      _cache.canonicalLigands
    );

    // Get list of tetrahedra from symmetry
    const auto& tetrahedraList = Symmetry::tetrahedra(_symmetry);

    precursors.reserve(tetrahedraList.size());
    for(const auto& tetrahedron : tetrahedraList) {
      /* Replace indices (represent positions within the symmetry) with the
       * ligand index at that position from the inverted map
       */

      // Make a minimal sequence from it
      precursors.push_back(
        temple::map(
          tetrahedron,
          [&](const boost::optional<unsigned>& indexOptional) -> boost::optional<unsigned> {
            if(indexOptional) {
              return symmetryPositionToLigandIndexMap.at(
                indexOptional.value()
              );
            }

            return boost::none;
          }
        )
      );
    }
  }

  return precursors;
}

std::vector<DistanceGeometry::ChiralityConstraint> AtomStereopermutator::Impl::chiralityConstraints(
  const double looseningMultiplier
) const {
  const double angleVariance = (
    DistanceGeometry::SpatialModel::angleAbsoluteVariance
    * looseningMultiplier
  );

  return temple::map(
    minimalChiralityConstraints(),
    [&](const auto& minimalConstraint) -> DistanceGeometry::ChiralityConstraint {
      /* We need to calculate target upper and lower volumes for the chirality
       * constraints. _cache.ligandDistances contains bounds for the distance to
       * each ligand site plane, and since the center of each cone should
       * constitute the average ligand position, we can calculate 1-3 distances
       * between the centerpoints of ligands using the idealized angles.
       *
       * The target volume of the chirality constraint created by the
       * tetrahedron is calculated using internal coordinates (the
       * Cayley-Menger determinant), always leading to V > 0, so depending on
       * the current assignment, the sign of the result is switched. The
       * formula used later in chirality constraint calculation for explicit
       * coordinates is adjusted by V' = 6 V to avoid an unnecessary factor, so
       * we do that here too:
       *
       *    288 V²  = |...|               | substitute V' = 6 V
       * -> 8 (V')² = |...|
       * ->      V' = sqrt(|...| / 8)
       *
       * where the Cayley-Menger determinant |...| is square symmetric:
       *
       *          |   0    1    1    1    1  |
       *          |        0  d12² d13² d14² |
       *  |...| = |             0  d23² d24² |
       *          |                  0  d34² |
       *          |  ...                  0  |
       *
       */

      using DeterminantMatrix = Eigen::Matrix<double, 5, 5>;

      DeterminantMatrix lowerMatrix, upperMatrix;

      lowerMatrix.row(0).setOnes();
      upperMatrix.row(0).setOnes();

      lowerMatrix.diagonal().setZero();
      upperMatrix.diagonal().setZero();

      /* Cycle through all combinations of ligand indices in the tetrahedron
       * definition sequence. boost::none means the central atom.
       */
      for(unsigned i = 0; i < 4; ++i) {
        boost::optional<DistanceGeometry::ValueBounds> iBounds;
        if(minimalConstraint.at(i)) {
          iBounds = _cache.ligandDistances.at(
            minimalConstraint.at(i).value()
          );
        }

        for(unsigned j = i + 1; j < 4; ++j) {
          boost::optional<DistanceGeometry::ValueBounds> jBounds;
          if(minimalConstraint.at(j)) {
            jBounds = _cache.ligandDistances.at(
              minimalConstraint.at(j).value()
            );
          }

          assert(iBounds || jBounds);

          DistanceGeometry::ValueBounds oneThreeDistanceBounds;
          if(iBounds && jBounds) {
            /* If neither index is the central atom, we can calculate an
             * expected one-three distance
             */
            double siteAngle = angle(
              minimalConstraint.at(i).value(),
              minimalConstraint.at(j).value()
            );

            oneThreeDistanceBounds = {
              CommonTrig::lawOfCosines(
                iBounds.value().lower,
                jBounds.value().lower,
                std::max(0.0, siteAngle - angleVariance)
              ),
              CommonTrig::lawOfCosines(
                iBounds.value().upper,
                jBounds.value().upper,
                std::min(M_PI, siteAngle + angleVariance)
              )
            };
          } else if(iBounds) {
            oneThreeDistanceBounds = iBounds.value();
          } else {
            oneThreeDistanceBounds = jBounds.value();
          }

          lowerMatrix(i + 1, j + 1) = std::pow(oneThreeDistanceBounds.lower, 2);
          upperMatrix(i + 1, j + 1) = std::pow(oneThreeDistanceBounds.upper, 2);
        }
      }

      const double boundFromLower = static_cast<DeterminantMatrix>(
        lowerMatrix.selfadjointView<Eigen::Upper>()
      ).determinant();

      const double boundFromUpper = static_cast<DeterminantMatrix>(
        upperMatrix.selfadjointView<Eigen::Upper>()
      ).determinant();

      assert(boundFromLower > 0 && boundFromUpper > 0);

      const double volumeFromLower = std::sqrt(boundFromLower / 8);
      const double volumeFromUpper = std::sqrt(boundFromUpper / 8);

      // Map the ligand indices to their constituent indices for use in the prototype
      auto tetrahedronLigands = temple::map(
        minimalConstraint,
        [&](const boost::optional<unsigned>& ligandIndexOptional) -> std::vector<AtomIndex> {
          if(ligandIndexOptional) {
            return _ranking.ligands.at(ligandIndexOptional.value());
          }

          return {_centerAtom};
        }
      );

      /* Although it is tempting to assume that the Cayley-Menger determinant
       * using the lower bounds is smaller than the one using upper bounds,
       * this is not always true. We cannot a priori know which of both yields
       * the lower or upper bounds on the 3D volume, and hence must ensure only
       * that the ordering is preserved in the generation of the constraint,
       * which checks that the lower bound on the volume is smaller than the
       * upper one.
       *
       * You can check this assertion with a CAS. The relationship between both
       * determinants (where u_ij = l_ij + Δ) is wholly indeterminant, i.e. no
       * logical operator (<, >, <=, >=, ==) between both is true. It
       * completely depends on the individual values. Maybe in very specific
       * cases one can deduce some relationship, but not generally.
       *
       * Also, since chemical_symmetry only emits positive chiral target volume
       * index sequences (see test case name allTetrahedraPositive), no
       * inversion has to be considered.
       */

      return {
        std::move(tetrahedronLigands),
        std::min(volumeFromLower, volumeFromUpper),
        std::max(volumeFromLower, volumeFromUpper)
      };
    }
  );
}

std::string AtomStereopermutator::Impl::info() const {
  std::string returnString = "A on "s
    + std::to_string(_centerAtom) + " ("s + Symmetry::name(_symmetry) +", "s;

  const auto& characters = _cache.symbolicCharacters;
  std::copy(
    characters.begin(),
    characters.end(),
    std::back_inserter(returnString)
  );

  for(const auto& link : _cache.selfReferentialLinks) {
    returnString += ", "s + characters.at(link.first) + "-"s + characters.at(link.second);
  }

  returnString += "): "s;

  if(_assignmentOption) {
    returnString += std::to_string(_assignmentOption.value());
  } else {
    returnString += "u";
  }

  const unsigned A = numAssignments();
  returnString += "/"s + std::to_string(A);

  const unsigned P = numStereopermutations();
  if(P != A) {
    returnString += " ("s + std::to_string(P) + ")"s;
  }

  return returnString;
}

std::string AtomStereopermutator::Impl::rankInfo() const {
  /* rankInfo is specifically geared towards RankingTree's consumption,
   * and MUST use indices of permutation
   */
  return (
    "CN-"s + std::to_string(static_cast<unsigned>(_symmetry))
    + "-"s + std::to_string(numStereopermutations())
    + "-"s + (
      indexOfPermutation()
      ? std::to_string(indexOfPermutation().value())
      : "u"s
    )
  );
}

unsigned AtomStereopermutator::Impl::numAssignments() const {
  return _cache.feasiblePermutations.size();
}

unsigned AtomStereopermutator::Impl::numStereopermutations() const {
  return _cache.permutations.assignments.size();
}

void AtomStereopermutator::Impl::setSymmetry(
  const Symmetry::Name symmetryName,
  const OuterGraph& graph
) {
  _symmetry = symmetryName;

  _cache = PermutationState {
    _ranking,
    _centerAtom,
    _symmetry,
    graph
  };

  // Dis-assign the stereopermutator
  assign(boost::none);
}

bool AtomStereopermutator::Impl::operator == (const AtomStereopermutator::Impl& other) const {
  return (
    _symmetry == other._symmetry
    && _centerAtom == other._centerAtom
    && numStereopermutations() == other.numStereopermutations()
    && _assignmentOption == other._assignmentOption
  );
}

bool AtomStereopermutator::Impl::operator < (const AtomStereopermutator::Impl& other) const {
  unsigned thisAssignments = numAssignments(),
           otherAssignments = other.numAssignments();
  /* Sequentially compare individual components, comparing assignments last
   * if everything else matches
   */
  return (
    std::tie( _centerAtom, _symmetry, thisAssignments, _assignmentOption)
    < std::tie(other._centerAtom, other._symmetry, otherAssignments, other._assignmentOption)
  );
}

} // namespace molassembler