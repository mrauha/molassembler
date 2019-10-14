/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief Compute the set of abstract permutations
 */

#ifndef INCLUDE_MOLASSEMBLER_STEREOPERMUTATIONS_ABSTRACT_PERMUTATIONS_H
#define INCLUDE_MOLASSEMBLER_STEREOPERMUTATIONS_ABSTRACT_PERMUTATIONS_H

#include "molassembler/RankingInformation.h"

#include "stereopermutation/GenerateUniques.h"

namespace Scine {
namespace molassembler {

/**
 * @brief Class to compute the set of abstract permutations from ranking
 *   and shape
 */
struct AbstractStereopermutations {
//!@name Static functions
//!@{
  /*!
   * @brief Stably re-sort ranked site indices in decreasing set size
   *
   * Necessary to avoid treating e.g. AAB and ABB separately, although the
   * resulting assignments are identical.
   *
   * @complexity{@math{N \log N}}
   *
   * Example:
   * @verbatim
   * rankedSites = {5, 8}, {3}, {1, 2, 4}
   * canonicalize(rankedSites) = {1, 2, 4}, {5, 8}, {3}
   * @endverbatim
   */
  static RankingInformation::RankedSitesType canonicalize(
    RankingInformation::RankedSitesType rankedSites
  );

  /*!
   * @brief Condense site ranking information into canonical characters for
   *   symbolic computation
   *
   * Use the output of canonicalize here as input:
   *
   * @complexity{@math{\Theta(N)}}
   *
   * Example:
   * @verbatim
   * rankedSites = {5, 8}, {3}, {1, 2, 4}
   * canonical = canonicalize(rankedSites = {1, 2, 4}, {5, 8}, {3}
   * transferToSymbolicCharacetrs(canonical) = A, A, A, B, B, C
   * @endverbatim
   */
  static std::vector<char> transferToSymbolicCharacters(
    const RankingInformation::RankedSitesType& canonicalSites
  );

  /*!
   * @brief Make site-index based links self-referential within canonical sites
   *
   * @complexity{@math{\Theta(L)}}
   *
   * Example:
   * @verbatim
   * links = {indexPair = {5, 8}}
   *
   * self-refential idx: 0  1  2    3  4    5
   * canonicalSites = {1, 2, 4}, {5, 8}, {3} (this is output from canonicalize)
   *
   * selfReferentialTransform(links, canonicalSites) = {3, 4}
   * @endverbatim
   */
  static stereopermutation::Stereopermutation::LinksSetType selfReferentialTransform(
    const std::vector<LinkInformation>& rankingLinks,
    const RankingInformation::RankedSitesType& canonicalSites
  );

  /*!
   * @brief Generates the reduced character representation of sites at their
   *   current shape positions
   *
   * @complexity{@math{O(S^2)} worst case}
   */
  static std::vector<char> makeStereopermutationCharacters(
    const RankingInformation::RankedSitesType& canonicalSites,
    const std::vector<char>& canonicalStereopermutationCharacters,
    const std::vector<unsigned>& sitesAtSymmetryPositions
  );
//!@}

//!@name Constructors
//!@{
  //! Empty initializer, all data members are nulled
  AbstractStereopermutations() = default;

  /**
   * @brief Generates the set of abstract stereopermutations and intermediate
   *   data
   *
   * @complexity{The generation of permutations dominates: @math{\Theta(S!)}}
   *
   * @param ranking Ranking object indicating chemical differences between
   *    substituents and sites
   * @param shape Shape name
   */
  AbstractStereopermutations(
    const RankingInformation& ranking,
    Symmetry::Shape shape
  );
//!@}

//!@name Data members
//!@{
  //! Stably resorted (by set size) site ranking
  RankingInformation::RankedSitesType canonicalSites;

  //! Character representation of bonding case
  std::vector<char> symbolicCharacters;

  //! Self-referential representation of links
  stereopermutation::Stereopermutation::LinksSetType selfReferentialLinks;

  //! Vector of rotationally unique stereopermutations with associated weights
  stereopermutation::StereopermutationsWithWeights permutations;
//!@}
};

} // namespace molassembler
} // namespace Scine


#endif
