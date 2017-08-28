#include "Properties.h"

#include <Eigen/Dense>

#include "template_magic/Numeric.h"
#include "template_magic/VectorView.h"
#include "template_magic/MemberFetcher.h"

namespace Symmetry {

namespace properties {

std::vector<unsigned> applyRotation(
  const std::vector<unsigned>& indices,
  const Symmetry::Name& symmetryName,
  unsigned rotationFunctionIndex
) {
  std::vector<unsigned> retv;

  for(
    const auto& index : 
    Symmetry::rotations(symmetryName).at(rotationFunctionIndex)
  ) {
    retv.push_back(
      indices.at(index)
    );
  }
  
  return retv;
}

Eigen::Vector3d getCoordinates(
  const Symmetry::Name& symmetryName,
  const boost::optional<unsigned>& indexInSymmetryOption
) {
  assert(
    (
      indexInSymmetryOption 
      && indexInSymmetryOption.value() < Symmetry::size(symmetryName)
    ) || !indexInSymmetryOption
  );

  if(indexInSymmetryOption) {
    return Symmetry::symmetryData().at(symmetryName).coordinates.at(
      indexInSymmetryOption.value()
    );
  }

  return {0, 0, 0};
}

double getTetrahedronVolume(
  const Eigen::Vector3d& i,
  const Eigen::Vector3d& j,
  const Eigen::Vector3d& k,
  const Eigen::Vector3d& l
) {
  return (
    i - l
  ).dot(
    (j - l).cross(k - l)
  );
}

double calculateAngleDistortion(
  const Symmetry::Name& from,
  const Symmetry::Name& to,
  const std::vector<unsigned>& indexMapping
) {
  const unsigned mappingIndexLimit = std::min(
    Symmetry::size(from),
    Symmetry::size(to)
  );

  assert(indexMapping.size() >= mappingIndexLimit);
  assert(
    std::abs(
      static_cast<int>(Symmetry::size(from)) - static_cast<int>(Symmetry::size(to))
    ) <= 1
  );

  double angularDistortion = 0;

  for(unsigned i = 0; i < mappingIndexLimit; ++i) {
    for(unsigned j = i + 1; j < mappingIndexLimit; ++j) {
      angularDistortion += std::fabs(
        Symmetry::angleFunction(from)(i, j)
        - Symmetry::angleFunction(to)(
          indexMapping.at(i),
          indexMapping.at(j)
        )
      );
    }
  }

  return angularDistortion;
}

boost::optional<unsigned> propagateIndexOptionalThroughMapping(
  const boost::optional<unsigned>& indexOptional,
  const std::vector<unsigned>& indexMapping
) {
  if(indexOptional) {
    return indexMapping.at(indexOptional.value());
  }

  return boost::none;
}


double calculateChiralDistortion(
  const Symmetry::Name& from,
  const Symmetry::Name& to,
  const std::vector<unsigned>& indexMapping
) {

  assert(
    indexMapping.size() >= std::min(
      Symmetry::size(from),
      Symmetry::size(to)
    )
  );

  double chiralDistortion = 0;

  // TODO probably erroneous here since tetrahedra in the source may involve
  // deleted indices in ligand loss situations!

  for(const auto& tetrahedron : Symmetry::tetrahedra(from)) {
    chiralDistortion += std::fabs(
      getTetrahedronVolume(
        getCoordinates(from, tetrahedron.at(0)),
        getCoordinates(from, tetrahedron.at(1)),
        getCoordinates(from, tetrahedron.at(2)),
        getCoordinates(from, tetrahedron.at(3))
      ) - getTetrahedronVolume(
        getCoordinates(
          to,
          propagateIndexOptionalThroughMapping(tetrahedron.at(0), indexMapping)
        ),
        getCoordinates(
          to,
          propagateIndexOptionalThroughMapping(tetrahedron.at(1), indexMapping)
        ),
        getCoordinates(
          to,
          propagateIndexOptionalThroughMapping(tetrahedron.at(2), indexMapping)
        ),
        getCoordinates(
          to,
          propagateIndexOptionalThroughMapping(tetrahedron.at(3), indexMapping)
        )
      )
    );
  }

  return chiralDistortion;
}


std::set<
  std::vector<unsigned>
> generateAllRotations(
  const Symmetry::Name& symmetryName,
  const std::vector<unsigned>& indices
) {
  // Idea: Tree-like expansion of all possible combinations of rotations.
  using IndicesList = std::vector<unsigned>;

  std::set<IndicesList> allRotations = {indices};

  /* We keep a chain of all applied rotations that led to a specific index
   * sequence.  The upper limit for every link in the chain is the number of
   * rotations in the symmetry
   */
  unsigned linkLimit = Symmetry::rotations(symmetryName).size();

  std::vector<unsigned> chain = {0};
  /* It's also necessary to keep the structures themselves since we may
   * backtrack and apply a new rotation to a previous structure. Keeping track
   * is cheaper than applying all rotations in the chain to the initial indices
   */
  std::vector<IndicesList> chainStructures = {indices};

  /* Loop is broken when the very first link in the chain has been incremented
   * to the link limit
   */
  while(chain.front() < linkLimit) {
    // perform rotation
    // copy the last element in chainStructures
    auto generated = applyRotation(
      chainStructures.back(),
      symmetryName,
      chain.back()
    );

    // is it something new?
    if(allRotations.count(generated) == 0) {
      // then add it to the set
      allRotations.insert(generated);

      // add it to the chain
      chainStructures.push_back(generated);
      chain.emplace_back(0);
    } else {
      // collapse the chain until we are at an incrementable position (if need be)
      while(
        chain.size() > 1 // retain at least first link in chain
        && chain.back() == linkLimit - 1 // remove link only if just below limit
      ) {
        chain.pop_back();
        chainStructures.pop_back();
      }

      // increment last position in chain
      ++chain.back(); 
    }
  }

  return allRotations;
}

std::vector<unsigned> applyIndexMapping(
  const Symmetry::Name& to,
  const std::vector<unsigned>& mapping
) {
  std::vector<unsigned> symmetryPositions (Symmetry::size(to));

  // TODO erroneous here for case of ligand loss, rethink!

  for(unsigned i = 0; i < Symmetry::size(to); ++i) {
    symmetryPositions.at(
      mapping.at(i)
    ) = i;
  }
  
  return symmetryPositions;
}

DistortionInfo::DistortionInfo(
  const std::vector<unsigned>& passIndexMapping,
  const double& passTotalDistortion,
  const double& passChiralDistortion
) : indexMapping(passIndexMapping),
    totalDistortion(passTotalDistortion),
    chiralDistortion(passChiralDistortion)
{}

SymmetryTransitionGroup::SymmetryTransitionGroup(
  const std::vector<
    std::vector<unsigned>
  >& passIndexMappings,
  const double& passAngleDistortion,
  const double& passChiralDistortion
) : indexMappings(passIndexMappings),
    angularDistortion(passAngleDistortion),
    chiralDistortion(passChiralDistortion) 
{}

SymmetryTransitionGroup symmetryTransitionMappings(
  const Symmetry::Name& symmetryFrom,
  const Symmetry::Name& symmetryTo
) {

  /* Symmetries must be adjacent in size (0 = rearrangement, 
   * +1 = ligand gain. Ligand loss is a special case where a specific position
   * in the symmetry group is removed, and is not covered here!
   */
  assert(
    (std::set<int> {0, 1}).count(
      static_cast<int>(Symmetry::size(symmetryTo))
      - static_cast<int>(Symmetry::size(symmetryFrom))
    ) == 1
  );

  /* Base idea: We need to go through all possible mappings. In situations where
   * the target symmetry has one more or one fewer ligand, the last index in 
   * the current sequence is either the added or removed ligand, and merely the
   * others are used to calculate the angular and chiral distortions involved
   * in the transition.
   *
   * For instance, from linear to T-shaped
   *
   *   0 - ( ) - 1     ->   0 - ( ) - 2
   *                             |
   *                             1
   *
   *   A mapping of {0 1 2} means that the new ligand is inserted at the 2 
   *   position of T-shaped, which would involve distorting the 0-1 angle by
   *   90°. The optimal mapping would be {0 2 1} (or its equivalent rotation 
   *   {2 0 1}), which does not have any angular distortion.
   */

  const unsigned largerSize = std::max(
    Symmetry::size(symmetryFrom),
    Symmetry::size(symmetryTo)
  );

  std::vector<DistortionInfo> distortions;

  auto indexMapping = detail::iota<unsigned>(largerSize);

  // Need to keep track of rotations of mappings to avoid repetition
  std::set<
    std::vector<unsigned>
  > encounteredSymmetryMappings;

  /* Using std::next_permutation generates all possible mappings!
   * do-while is required since the very first mapping must be considered before
   * calling next_permutation for the first time
   */
  do {
    if( // is the mapping new?
      encounteredSymmetryMappings.count(
        applyIndexMapping(
          symmetryTo,
          indexMapping
        )
      ) == 0
    ) {
      /* Add it to the set of possible distortions, calculate angular and
       * chiral distortion involved in the mapping
       */
      distortions.emplace_back(
        indexMapping,
        calculateAngleDistortion(symmetryFrom, symmetryTo, indexMapping),
        calculateChiralDistortion(symmetryFrom, symmetryTo, indexMapping)
      );

      /* Any rotations of the mapping in the target symmetry are equivalent, we
       * do not want to count these as an additional multiplicity, so we 
       * generate them and add them to the encountered mappings
       */
      auto allRotations = generateAllRotations(
        symmetryTo,
        applyIndexMapping(
          symmetryTo,
          indexMapping
        )
      );

      encounteredSymmetryMappings.insert(
        allRotations.begin(),
        allRotations.end()
      );
    }
  } while (std::next_permutation(indexMapping.begin(), indexMapping.end()));

  /* We are interested only in the those transitions that have the very lowest
   * angular distortion, and within that set only the lowest chiral distortion,
   * so we sub-select within the generated set
   */

  double lowestAngularDistortion = TemplateMagic::min(
    TemplateMagic::getMember(
      distortions,
      [](const auto& distortion) -> double {
        return distortion.totalDistortion;
      }
    )
  );

  auto distortionsView = TemplateMagic::filter(
    distortions,
    [&lowestAngularDistortion](const auto& distortion) -> bool {
      return distortion.totalDistortion > lowestAngularDistortion;
    }
  );

  double lowestChiralDistortion = TemplateMagic::min(
    TemplateMagic::getMember(
      distortionsView,
      [](const auto& distortion) -> double {
        return distortion.chiralDistortion;
      }
    )
  );

  // continue filtering on lowest distortions
  distortionsView.filter(
    [&lowestChiralDistortion](const auto& distortion) -> bool {
      return distortion.chiralDistortion > lowestChiralDistortion;
    }
  );

  return SymmetryTransitionGroup(
    TemplateMagic::mapToVector( // copy out index mappings
      distortionsView,
      [](const auto& distortionInfo) -> std::vector<unsigned> {
        return distortionInfo.indexMapping;
      }
    ),
    lowestAngularDistortion,
    lowestChiralDistortion
  );
}

SymmetryTransitionGroup ligandLossTransitionMappings(
  const Symmetry::Name& symmetryFrom,
  const Symmetry::Name& symmetryTo,
  const unsigned& positionInSourceSymmetry
) {
  // Ensure we are dealing with ligand loss
  assert(Symmetry::size(symmetryTo) + 1 == Symmetry::size(symmetryFrom));
  assert(positionInSourceSymmetry < Symmetry::size(symmetryFrom));

  std::vector<unsigned> indexMapping = TemplateMagic::concatenate(
    detail::iota<unsigned>(positionInSourceSymmetry),
    detail::range(positionInSourceSymmetry + 1, Symmetry::size(symmetryFrom))
  );

  indexMapping.push_back(positionInSourceSymmetry);

  std::set<
    std::vector<unsigned>
  > encounteredSymmetryMappings;
}

// NOTE - is deprecated, symmetryTransitionMappings aims to be the general case
SymmetryTransitionGroup ligandGainDistortions(
  const Symmetry::Name& symmetryFrom,
  const Symmetry::Name& symmetryTo
) {
  assert(Symmetry::size(symmetryTo) == Symmetry::size(symmetryFrom) + 1);

  std::vector<DistortionInfo> distortions;

  // Create a vector of indices for the new symmetry to use as mapping
  std::vector<unsigned> indexMapping (Symmetry::size(symmetryTo));
  std::iota(
    indexMapping.begin(),
    indexMapping.end(),
    0
  );

  std::set<
    std::vector<unsigned>
  > encounteredSymmetryMappings;

  do {
    if(
      encounteredSymmetryMappings.count(
        applyIndexMapping(
          symmetryTo,
          indexMapping
        )
      ) == 0
    ) {
      distortions.emplace_back(
        indexMapping,
        calculateAngleDistortion(symmetryFrom, symmetryTo, indexMapping),
        calculateChiralDistortion(symmetryFrom, symmetryTo, indexMapping)
      );

      auto allRotations = generateAllRotations(
        symmetryTo,
        applyIndexMapping(
          symmetryTo,
          indexMapping
        )
      );

      encounteredSymmetryMappings.insert(
        allRotations.begin(),
        allRotations.end()
      );
    }
  } while (std::next_permutation(indexMapping.begin(), indexMapping.end()));

  double lowestAngularDistortion = TemplateMagic::min(
    TemplateMagic::getMember(
      distortions,
      [](const auto& distortion) -> double {
        return distortion.totalDistortion;
      }
    )
  );

  auto distortionsView = TemplateMagic::filter(
    distortions,
    [&lowestAngularDistortion](const auto& distortion) -> bool {
      return distortion.totalDistortion > lowestAngularDistortion;
    }
  );

  double lowestChiralDistortion = TemplateMagic::min(
    TemplateMagic::getMember(
      distortionsView,
      [](const auto& distortion) -> double {
        return distortion.chiralDistortion;
      }
    )
  );

  // continue filtering on lowest distortions
  distortionsView.filter(
    [&lowestChiralDistortion](const auto& distortion) -> bool {
      return distortion.chiralDistortion > lowestChiralDistortion;
    }
  );

  return SymmetryTransitionGroup(
    TemplateMagic::mapToVector( // copy out index mappings
      distortionsView,
      [](const auto& distortionInfo) -> std::vector<unsigned> {
        return distortionInfo.indexMapping;
      }
    ),
    lowestAngularDistortion,
    lowestChiralDistortion
  );
}

} // namespace properties

} // namespace Symmetry
