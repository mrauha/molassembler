/* @file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher PointGroup.
 *   See LICENSE.txt for details.
 */

#define BOOST_TEST_MODULE SymmetryTests

#include <boost/test/unit_test.hpp>
#include <Eigen/Geometry>

#include "temple/Adaptors/AllPairs.h"
#include "temple/Adaptors/Zip.h"
#include "temple/constexpr/Numeric.h"
#include "temple/constexpr/ToSTL.h"
#include "temple/constexpr/TupleTypePairs.h"
#include "temple/Functional.h"
#include "temple/SetAlgorithms.h"
#include "temple/Stringify.h"
#include "temple/Functor.h"

#include "chemical_symmetries/Partitioner.h"
#include "chemical_symmetries/Properties.h"
#include "chemical_symmetries/Recognition.h"
#include "chemical_symmetries/Symmetries.h"

#include <set>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <numeric>

using namespace Scine;
using namespace Symmetry;

template<typename EnumType>
constexpr auto underlying(const EnumType e) {
  return static_cast<std::underlying_type_t<EnumType>>(e);
}

std::vector<unsigned> rotate(
  const std::vector<unsigned>& toRotate,
  const std::vector<unsigned>& rotationVector
) {
  std::vector<unsigned> rotated (toRotate.size());

  for(unsigned i = 0; i < toRotate.size(); i++) {
    rotated[i] = toRotate[
      rotationVector[i]
    ];
  }

  return rotated;
}

template<typename SymmetryClass>
struct LockstepTest {
  static bool value() {
    return Symmetry::nameIndex(SymmetryClass::name) == underlying(SymmetryClass::name);
  }
};

BOOST_AUTO_TEST_CASE(symmetryTypeAndPositionInEnumLockstep) {
  BOOST_CHECK_MESSAGE(
    temple::all_of(
      temple::TupleType::map<
        Symmetry::data::allSymmetryDataTypes,
        LockstepTest
      >(),
      temple::Identity {}
    ),
    "Not all symmetries have the same order in Name and allSymmetryDataTypes"
  );
}

BOOST_AUTO_TEST_CASE(symmetryDataConstructedCorrectly) {
  BOOST_TEST_REQUIRE(Symmetry::symmetryData().size() == Symmetry::nSymmetries);

  BOOST_REQUIRE(
    temple::all_of(
      Symmetry::allNames,
      [&](const auto& symmetryName) -> bool {
        return Symmetry::symmetryData().count(symmetryName) == 1;
      }
    )
  );
}

BOOST_AUTO_TEST_CASE(angleFuntionsInSequence) {
  BOOST_CHECK(
    temple::all_of(
      temple::adaptors::zip(
        Symmetry::data::angleFunctions,
        std::vector<Symmetry::data::AngleFunctionPtr> {{
          &Symmetry::data::Linear::angleFunction,
          &Symmetry::data::Bent::angleFunction,
          &Symmetry::data::TrigonalPlanar::angleFunction, // 3
          &Symmetry::data::CutTetrahedral::angleFunction,
          &Symmetry::data::TShaped::angleFunction,
          &Symmetry::data::Tetrahedral::angleFunction, // 4
          &Symmetry::data::SquarePlanar::angleFunction,
          &Symmetry::data::Seesaw::angleFunction,
          &Symmetry::data::TrigonalPyramidal::angleFunction,
          &Symmetry::data::SquarePyramidal::angleFunction, // 5
          &Symmetry::data::TrigonalBiPyramidal::angleFunction,
          &Symmetry::data::PentagonalPlanar::angleFunction,
          &Symmetry::data::Octahedral::angleFunction, // 6
          &Symmetry::data::TrigonalPrismatic::angleFunction,
          &Symmetry::data::PentagonalPyramidal::angleFunction,
          &Symmetry::data::PentagonalBiPyramidal::angleFunction, // 7
          &Symmetry::data::SquareAntiPrismatic::angleFunction // 8
        }}
      ),
      [](const auto& aPtr, const auto& bPtr) -> bool {
        return aPtr == bPtr;
      }
    )
  );
}

BOOST_AUTO_TEST_CASE( correctRotationVectorSize ) {
  // every rotation vector size must equal size of symmetry
  for(const auto& name : allNames) {
    for(const auto& rotationVector : rotations(name)) {
      BOOST_CHECK(rotationVector.size() == size(name));
    }
  }

}

BOOST_AUTO_TEST_CASE( rotationVectorSanityTests ) {

  // every rotation may have every number 0 -> (size of symmetry - 1) only once
  for(const auto& name : allNames) {
    std::set<unsigned> members;
    for(unsigned i = 0; i < size(name); i++) {
      members.insert(members.end(), i);
    }

    for(const auto& rotationVector : rotations(name)) {
      std::set<unsigned> converted {
        rotationVector.begin(),
        rotationVector.end()
      };

      BOOST_CHECK(converted.size() == size(name)); // no duplicates

      BOOST_CHECK(
        std::accumulate(
          rotationVector.begin(),
          rotationVector.end(),
          true,
          [&members](const bool carry, const unsigned rotationElement) {
            return carry && (members.count(rotationElement) == 1);
          }
        )
      );
    }
  }

  /* every rotation must return to the original after a finite number of
   * applications
   */
  unsigned maxIter = 100;
  for(const auto& name : allNames) {
    std::vector<unsigned> initialConfiguration (
      size(name),
      0
    );

    std::iota(
      initialConfiguration.begin(),
      initialConfiguration.end(),
      0
    );

    for(const auto& rotationVector : rotations(name)) {
      // copy in from initial
      auto configuration = initialConfiguration;

      bool pass = false;
      for(unsigned N = 0; N < maxIter; N++) {
        configuration = rotate(configuration, rotationVector);
        if(configuration == initialConfiguration) {
          pass = true;
          break;
        }
      }

      BOOST_CHECK(pass);
    }
  }

}

BOOST_AUTO_TEST_CASE( angleFunctionInputSymmetry ) {

  // every angle function must be symmetrical on input of valid unsigned indices
  for(const auto& symmetryName: allNames) {
    bool passesAll = true;

    for(unsigned i = 0; i < size(symmetryName) && passesAll; i++) {
      for(unsigned j = i + 1; j < size(symmetryName); j++) {
        if(angleFunction(symmetryName)(i, j) != angleFunction(symmetryName)(j, i)) {
          passesAll = false;
          std::cout << name(symmetryName)
            << " is not symmetrical w.r.t. input indices: falsified by ("
            << i << ", " << j <<") -> (" << angleFunction(symmetryName)(i, j)
            << ", " << angleFunction(symmetryName)(j, i) << ")." << std::endl;
          break;
        }
      }
    }

    BOOST_CHECK(passesAll);
  }

}

BOOST_AUTO_TEST_CASE( angleFunctionZeroForIdenticalInput) {

  // every angle function must return 0 for identical indices
  for(const auto& symmetryName: allNames) {
    bool passesAll = true;

    for(unsigned i = 0; i < size(symmetryName); i++) {
      if(angleFunction(symmetryName)(i, i) != 0) {
        passesAll = false;
        std::cout << name(symmetryName)
          << "'s angle function does not return zero for identical indices ("
          << i << ", " << i << ")." << std::endl;
        break;
      }
    }

    BOOST_CHECK(passesAll);
  }
}

BOOST_AUTO_TEST_CASE(anglesWithinRadiansBounds) {
  for(const auto& symmetryName : allNames) {
    bool passesAll = true;

    for(unsigned i = 0; i < size(symmetryName); i++) {
      for(unsigned j = 0; j < size(symmetryName); j++) {
        if(
          !(
            0 <= angleFunction(symmetryName)(i, j)
          ) || !(
            angleFunction(symmetryName)(i, j) <= M_PI
          )
        ) {
          passesAll = false;
          std::cout << name(symmetryName)
            << "'s angle function is not within radians bounds for indices ("
            << i << ", " << j << ") -> " << angleFunction(symmetryName)(i, j)
            << std::endl;
          break;
        }
      }
    }

    BOOST_CHECK(passesAll);
  }
}

BOOST_AUTO_TEST_CASE( rightAmountOfCoordinates) {
  // every information must have the right amount of coordinates
  for(const auto& symmetryName: allNames) {
    BOOST_CHECK(
      symmetryData().at(symmetryName).coordinates.size() ==
      symmetryData().at(symmetryName).size
    );
  }
}

BOOST_AUTO_TEST_CASE( allCoordinateVectorsLengthOne) {
  for(const auto& symmetryName: allNames) {
    bool all_pass = true;

    for(const auto& coordinate: symmetryData().at(symmetryName).coordinates) {
      if(coordinate.norm() - 1 > 1e10) {
        all_pass = false;
        break;
      }
    }

    BOOST_CHECK(all_pass);
  }
}

BOOST_AUTO_TEST_CASE( anglesMatchCoordinates) {

  /* The results of the angle functions ought to match the geometries specified
   * by the coordinates
   */

  for(const auto& symmetryName: allNames) {
    auto getCoordinates =  [&](const unsigned index) -> Eigen::Vector3d {
      return symmetryData().at(symmetryName).coordinates.at(index);
    };

    bool all_pass = true;

    for(unsigned i = 0; i < size(symmetryName); i++) {
      for(unsigned j = i + 1; j < size(symmetryName); j++) {
        auto angleInCoordinates = std::acos(
          getCoordinates(i).dot(
            getCoordinates(j)
          ) / (
            getCoordinates(i).norm() * getCoordinates(j).norm()
          )
        );

        auto angleDifference = angleInCoordinates - angleFunction(symmetryName)(i, j);

        // Tolerate only one degree difference
        if(std::fabs(angleDifference) > 1) {
          all_pass = false;

          std::cout << name(symmetryName)
            << ": angleFunction != angles from coordinates ("
            << i << ", " << j << "): " << angleDifference
            << ", angleFunction = " << angleFunction(symmetryName)(i, j)
            << ", angle from coordinates = " << angleInCoordinates << std::endl;
        }
      }
    }

    BOOST_CHECK(all_pass);
  }
}

BOOST_AUTO_TEST_CASE( allTetrahedraPositive) {
  /* Checks if sequence that tetrahedra are defined in leads to a positive
   * volume when calculated via
   *
   *  (1 - 4) dot [ (2 - 4) x (3 - 4) ]
   *
   */
  for(const auto& symmetryName: allNames) {
    auto getCoordinates = [&](const boost::optional<unsigned>& indexOption) -> Eigen::Vector3d {
      if(indexOption) {
        return symmetryData().at(symmetryName).coordinates.at(indexOption.value());
      }

      return {0, 0, 0};
    };

    bool all_pass = true;

    for(const auto& tetrahedron: tetrahedra(symmetryName)) {

      double tetrahedronVolume = (
        getCoordinates(tetrahedron[0]) - getCoordinates(tetrahedron[3])
      ).dot(
        (
          getCoordinates(tetrahedron[1]) - getCoordinates(tetrahedron[3])
        ).cross(
          getCoordinates(tetrahedron[2]) - getCoordinates(tetrahedron[3])
        )
      );

      if(tetrahedronVolume < 0) {
        all_pass = false;
        std::cout << name(symmetryName) << ": Tetrahedron {";

        for(unsigned i = 0; i < 4; i++) {
          if(tetrahedron[i]) {
            std::cout << tetrahedron[i].value();
          } else {
            std::cout << "C";
          }

          if(i != 3) {
            std::cout << ", ";
          }
        }

        std::cout << "} has negative volume (" << tetrahedronVolume << ")."
          << std::endl;
      }
    }

    BOOST_CHECK(all_pass);
  }
}

BOOST_AUTO_TEST_CASE( tetrahedraDefinitionIndicesUnique ) {
  for(const auto& symmetryName : allNames) {
    for(const auto& tetrahedron : tetrahedra(symmetryName)) {
      bool containsAnEmptyOption = false;

      for(const auto& edgeOption : tetrahedron) {
        if(!edgeOption) {
          containsAnEmptyOption = true;
          break;
        }
      }

      std::set<unsigned> indices;

      for(const auto& edgeOption : tetrahedron) {
        if(edgeOption) {
          indices.insert(edgeOption.value());
        }
      }

      BOOST_CHECK(indices.size() + static_cast<unsigned>(containsAnEmptyOption) == 4);
    }
  }
}

BOOST_AUTO_TEST_CASE(smallestAngleValueCorrect) {
  const double comparisonSmallestAngle = temple::min(
    temple::map(
      allNames,
      [](const Name& symmetryName) -> double {
        double symmetrySmallestAngle = angleFunction(symmetryName)(0, 1);

        for(unsigned i = 0; i < size(symmetryName); i++) {
          for(unsigned j = i + 1; j < size(symmetryName); j++) {
            double angle = angleFunction(symmetryName)(i, j);
            if(angle < symmetrySmallestAngle) {
              symmetrySmallestAngle = angle;
            }
          }
        }

        return symmetrySmallestAngle;
      }
    )
  );

  BOOST_CHECK(0 < smallestAngle && smallestAngle < M_PI);
  BOOST_CHECK_MESSAGE(
    std::fabs(
      smallestAngle - comparisonSmallestAngle
    ) < 1e-4,
    "The constant smallest angle set by the library is NOT the smallest "
    << "returned angle within the library. Current value of smallestAngle: "
    << smallestAngle
    << ", true smallest angle:" << comparisonSmallestAngle
  );
}

#ifdef USE_CONSTEXPR_TRANSITION_MAPPINGS
/* NOTE: can refactor out doLigandGainTestIfAdjacent with a simple if-constexpr
 * in C++17
 */
template<class SymmetryClassFrom, class SymmetryClassTo>
std::enable_if_t<
  (
    SymmetryClassFrom::size + 1 == SymmetryClassTo::size
    || SymmetryClassFrom::size == SymmetryClassTo::size
  ),
  bool
> doLigandGainTestIfAdjacent() {
  /* Struct:
   * .mappings - dynamic array of fixed-size index mappings
   * .angularDistortion, .chiralDistortion - doubles
   */
  auto constexprMappings = allMappings.at(
    static_cast<unsigned>(SymmetryClassFrom::name),
    static_cast<unsigned>(SymmetryClassTo::name)
  ).value();
  /* Vector of structs:
   * .indexMapping - vector containing the index mapping
   * .totalDistortion, .chiralDistortion - doubles
   */
  auto dynamicMappings = properties::selectBestTransitionMappings(
    properties::symmetryTransitionMappings(
      SymmetryClassFrom::name,
      SymmetryClassTo::name
    )
  );

  temple::floating::ExpandedRelativeEqualityComparator<double> comparator {
    properties::floatingPointEqualityThreshold
  };

  if(
    comparator.isUnequal(
      dynamicMappings.angularDistortion,
      constexprMappings.angularDistortion
    ) || comparator.isUnequal(
      dynamicMappings.chiralDistortion,
      constexprMappings.chiralDistortion
    )
  ) {
    return false;
  }

  // Do a full set comparison
  auto convertedMappings = temple::map_stl(
    temple::toSTL(constexprMappings.mappings),
    [&](const auto& indexList) -> std::vector<unsigned> {
      return {
        std::begin(indexList),
        std::end(indexList)
      };
    }
  );

  decltype(convertedMappings) dynamicResultSet {
    dynamicMappings.indexMappings.begin(),
    dynamicMappings.indexMappings.end()
  };

  return temple::set_symmetric_difference(
    convertedMappings,
    dynamicResultSet
  ).empty();
}

using IndexAndMappingsPairType = std::pair<
  unsigned,
  Symmetry::constexprProperties::MappingsReturnType
>;

constexpr bool pairEqualityComparator(
  const IndexAndMappingsPairType& a,
  const IndexAndMappingsPairType& b
) {
  temple::floating::ExpandedRelativeEqualityComparator<double> comparator {
    Symmetry::properties::floatingPointEqualityThreshold
  };

  return (
    comparator.isEqual(a.second.angularDistortion, b.second.angularDistortion)
    && comparator.isEqual(a.second.chiralDistortion, b.second.chiralDistortion)
  );
}

template<class SymmetryClassFrom, class SymmetryClassTo>
std::enable_if_t<
  SymmetryClassFrom::size == SymmetryClassTo::size + 1,
  bool
> doLigandGainTestIfAdjacent() {
  // Ligand loss situation

  /* Constexpr part */
  temple::Array<
    std::pair<
      unsigned,
      Symmetry::constexprProperties::MappingsReturnType
    >,
    SymmetryClassFrom::size
  > constexprMappings;

  for(unsigned i = 0; i < SymmetryClassFrom::size; ++i) {
    constexprMappings.at(i) = std::make_pair(
      i,
      Symmetry::constexprProperties::ligandLossMappings<
        SymmetryClassFrom,
        SymmetryClassTo
      >(i)
    );
  }

  // Group the results
  auto constexprGroups = temple::groupByEquality(
    constexprMappings,
    pairEqualityComparator // C++17 constexpr lambda
  );

  /* Dynamic part */
  std::vector<
    std::pair<
      unsigned,
      Symmetry::properties::SymmetryTransitionGroup
    >
  > dynamicMappings;

  for(unsigned i = 0; i < SymmetryClassFrom::size; ++i) {
    dynamicMappings.emplace_back(
      i,
      selectBestTransitionMappings(
        properties::ligandLossTransitionMappings(
          SymmetryClassFrom::name,
          SymmetryClassTo::name,
          i
        )
      )
    );
  }

  // Analyze all mappings - which indices have "identical" target mappings?
  auto dynamicGroups = temple::groupByEquality(
    dynamicMappings,
    [&](const auto& firstMappingPair, const auto& secondMappingPair) -> bool {
      return (
        temple::floating::isCloseRelative(
          firstMappingPair.second.angularDistortion,
          secondMappingPair.second.angularDistortion,
          Symmetry::properties::floatingPointEqualityThreshold
        ) && temple::floating::isCloseRelative(
          firstMappingPair.second.chiralDistortion,
          secondMappingPair.second.chiralDistortion,
          Symmetry::properties::floatingPointEqualityThreshold
        )
      );
    }
  );

  /* Comparison */
  // Quick check
  if(dynamicGroups.size() != constexprGroups.size()) {
    return false;
  }

  // Compare an unsigned set of sub-group sizes from each
  std::multiset<unsigned> dynamicGroupSizes, constexprGroupSizes;

  for(const auto& dynamicGroup : dynamicGroups) {
    dynamicGroupSizes.insert(dynamicGroup.size());
  }

  for(const auto& constexprGroup : constexprGroups) {
    constexprGroupSizes.insert(constexprGroup.size());
  }

  return dynamicGroupSizes == constexprGroupSizes;
}

// Base case in which source and target symmetries are non-adjacent
template<class SymmetryClassFrom, class SymmetryClassTo>
std::enable_if_t<
  (
    SymmetryClassFrom::size != SymmetryClassTo::size + 1
    && SymmetryClassFrom::size + 1 != SymmetryClassTo::size
    && SymmetryClassFrom::size != SymmetryClassTo::size
  ),
  bool
> doLigandGainTestIfAdjacent() {
  return true;
}

template<class SymmetryClassFrom, class SymmetryClassTo>
struct LigandGainTest {
  static bool value() {
    return doLigandGainTestIfAdjacent<SymmetryClassFrom, SymmetryClassTo>();
  }
};
#endif

template<class SymmetryClass>
struct RotationGenerationTest {
  static bool value() {

    // This is a DynamicSet of SymmetryClass-sized Arrays
    auto constexprRotations = constexprProperties::generateAllRotations<SymmetryClass>(
      constexprProperties::startingIndexSequence<SymmetryClass>()
    );

    // This is a std::set of SymmetryClass-sized std::vectors
    auto dynamicRotations = properties::generateAllRotations(
      SymmetryClass::name,
      temple::iota<unsigned>(SymmetryClass::size)
    );

    auto convertedRotations = temple::map_stl(
      temple::toSTL(constexprRotations),
      [&](const auto& indexList) -> std::vector<unsigned> {
        return {
          indexList.begin(),
          indexList.end()
        };
      }
    );

    if(convertedRotations.size() != constexprRotations.size()) {
      std::cout << "In symmetry " << SymmetryClass::stringName << ", "
        << "constexpr rotations set reports " << constexprRotations.size()
        << " elements but the STL mapped variant has only "
        << convertedRotations.size() << " elements!" << std::endl;
    }

    bool pass = true;

    // Size mismatch
    if(convertedRotations.size() != dynamicRotations.size()) {
      pass = false;
    } else {
      pass = (
        temple::set_symmetric_difference(
          convertedRotations,
          dynamicRotations
        ).empty()
      );
    }

    if(!pass) {
      std::cout << "Rotation generation differs for "
        << SymmetryClass::stringName
        << " symmetry: Sizes of generated sets are different. "
        << "constexpr - " << convertedRotations.size() << " != "
        << dynamicRotations.size() << " - dynamic" << std::endl;
      std::cout << " Maximum #rotations: " << constexprProperties::maxRotations<SymmetryClass>()
        << std::endl;

      std::cout << " Converted constexpr:" << std::endl;
      for(const auto& element : convertedRotations) {
        std::cout << " {" << temple::condense(element)
          << "}\n";
      }

      std::cout << " Dynamic:" << std::endl;
      for(const auto& element : dynamicRotations) {
        std::cout << " {" << temple::condense(element)
          << "}\n";
      }
    }

    return pass;
  }
/* Previously, when the interface with which this is used (unpackToFunction) was
 * unable to cope with both value data members and value function members
 * equally, this was of the form::
 *
 *   static bool initialize() {
 *     ...
 *     return value;
 *   }
 *   static bool value = initialize();
 *
 * Although this seems equivalent, it really isn't, since initialize() is called
 * at static initialization time instead of at first use as when value is
 * a function. Since, in this case, the value function depends on another static
 * value (generateAllRotations -> symmetryData), the value-initialize variant
 * leads to a static initialization fiasco, where it is unclear whether the
 * value data member or symmetryData is initialized first.
 *
 * Only in the case of static constexpr is the value-initialize variant
 * semantically equivalent to the data member variant.
 */
};

std::string getGraphvizNodeName(const Symmetry::Name& symmetryName) {
  auto stringName = Symmetry::name(symmetryName);

  stringName.erase(
    std::remove_if(
      stringName.begin(),
      stringName.end(),
      [](const char& singleChar) -> bool {
        return (
          singleChar == ' '
          || singleChar == '-'
        );
      }
    ),
    stringName.end()
  );
  return stringName;
}

BOOST_AUTO_TEST_CASE(constexprPropertiesTests) {
  // Full test of rotation algorithm equivalency for all symmetries
  BOOST_CHECK_MESSAGE(
    temple::all_of(
      temple::TupleType::map<
        Symmetry::data::allSymmetryDataTypes,
        RotationGenerationTest
      >(),
      temple::Identity {}
    ),
    "There is a discrepancy between constexpr and dynamic rotation generation"
  );

#ifdef USE_CONSTEXPR_TRANSITION_MAPPINGS
  // Test transitions generation/evaluation algorithm equivalency for all
  BOOST_CHECK_MESSAGE(
    temple::all_of(
      temple::TupleType::mapAllPairs<
        Symmetry::data::allSymmetryDataTypes,
        LigandGainTest
      >(),
      temple::Identity {}
    ),
    "There is a discrepancy between constexpr and dynamic ligand gain mapping"
    << " generation!"
  );
#endif
}

template<typename SymmetryClass>
struct NumUnlinkedTestFunctor {
  static bool value() {
    for(unsigned i = 1; i < SymmetryClass::size; ++i) {
      unsigned constexprResult = constexprProperties::numUnlinkedStereopermutations<SymmetryClass>(i);

      unsigned dynamicResult = properties::numUnlinkedStereopermutations(
        SymmetryClass::name,
        i
      );

      if(constexprResult != dynamicResult) {
        std::cout << "Mismatch for " << Symmetry::name(SymmetryClass::name) << " and " << i << " identical ligands between constexpr and dynamic number of unlinked: " << constexprResult << " vs. " << dynamicResult << "\n";
        return false;
      }

      // Cross-check with constexpr hasMultiple
      bool constexprHasMultiple = constexprProperties::hasMultipleUnlinkedStereopermutations<SymmetryClass>(i);
      if((constexprResult > 1) != constexprHasMultiple) {
        std::cout << "Mismatch between constexpr count and constexpr "
          << "hasMultiple unlinked ligands for "
          << Symmetry::name(SymmetryClass::name) << " and "
          << i << " identical ligands: " << constexprResult << " and "
          << std::boolalpha << constexprHasMultiple << "\n";
        return false;
      }

      // Cross-check with dynamic hasMultiple
      bool dynamicHasMultiple = properties::hasMultipleUnlinkedStereopermutations(SymmetryClass::name, i);
      if((constexprResult > 1) != dynamicHasMultiple) {
        std::cout << "Mismatch between constexpr count and dynamic "
          << "hasMultiple unlinked ligands for "
          << Symmetry::name(SymmetryClass::name) << " and "
          << i << " identical ligands: " << constexprResult << " and "
          << std::boolalpha << dynamicHasMultiple << "\n";
        return false;
      }
    }

    return true;
  }
};

BOOST_AUTO_TEST_CASE(numUnlinkedAlgorithms) {
  BOOST_CHECK_MESSAGE(
    temple::all_of(
      temple::TupleType::map<
        Symmetry::data::allSymmetryDataTypes,
        NumUnlinkedTestFunctor
      >(),
      temple::Identity {}
    ),
    "Not all numbers of unlinked stereopermutations match across constexpr and dynamic"
    " algorithms"
  );

  BOOST_CHECK(properties::numUnlinkedStereopermutations(Symmetry::Name::Linear, 0) == 1);
  BOOST_CHECK(properties::numUnlinkedStereopermutations(Symmetry::Name::Bent, 0) == 1);
  BOOST_CHECK(properties::numUnlinkedStereopermutations(Symmetry::Name::TrigonalPlanar, 0) == 1);
  BOOST_CHECK(properties::numUnlinkedStereopermutations(Symmetry::Name::Tetrahedral, 0) == 2);
  BOOST_CHECK(properties::numUnlinkedStereopermutations(Symmetry::Name::Octahedral, 0) == 30);
}

static_assert(
  nSymmetries == std::tuple_size<data::allSymmetryDataTypes>::value,
  "nSymmetries does not equal number of symmetry data class types in "
  "allSymmetryDataTypes"
);

#ifdef USE_CONSTEXPR_TRANSITION_MAPPINGS
BOOST_AUTO_TEST_CASE(mappingsAreAvailable) {
  /* In every case where allMappings has a value, getMapping must also return
   * a some optional
   */
  bool pass = true;
  for(const auto& fromSymmetry : Symmetry::allNames) {
    auto i = static_cast<unsigned>(fromSymmetry);
    for(const auto& toSymmetry : Symmetry::allNames) {
      auto j = static_cast<unsigned>(toSymmetry);
      if(
        i < j
        && allMappings.at(i, j).hasValue()
          != static_cast<bool>(
            Symmetry::getMapping(fromSymmetry, toSymmetry)
          )
      ) {
        pass = false;
        break;
      }
    }
  }

  BOOST_CHECK_MESSAGE(
    pass,
    "Not all constexpr mappings from allMappings are available from getMapping!"
  );
}
#endif

BOOST_AUTO_TEST_CASE(angleBoundsTests) {
  BOOST_CHECK(Symmetry::minimumAngle(Symmetry::Name::TShaped) == M_PI / 2);
  BOOST_CHECK(Symmetry::maximumAngle(Symmetry::Name::TShaped) == M_PI);

  BOOST_CHECK(Symmetry::minimumAngle(Symmetry::Name::Octahedral) == M_PI / 2);
  BOOST_CHECK(Symmetry::maximumAngle(Symmetry::Name::Octahedral) == M_PI);

  BOOST_CHECK(Symmetry::minimumAngle(Symmetry::Name::TrigonalBiPyramidal) == M_PI / 2);
  BOOST_CHECK(Symmetry::maximumAngle(Symmetry::Name::TrigonalBiPyramidal) == M_PI);

  BOOST_CHECK(
    Symmetry::minimumAngle(Symmetry::Name::Tetrahedral) == Symmetry::maximumAngle(Symmetry::Name::Tetrahedral)
  );
}

BOOST_AUTO_TEST_CASE(Recognition) {
  const std::map<Name, PointGroup> expected {
    {Name::Linear, PointGroup::Dinfh},
    {Name::Bent, PointGroup::C2v},
    {Name::TrigonalPlanar, PointGroup::D3h},
    {Name::CutTetrahedral, PointGroup::C3v},
    {Name::TShaped, PointGroup::C2v},
    {Name::Tetrahedral, PointGroup::Td},
    {Name::SquarePlanar, PointGroup::D4h},
    {Name::Seesaw, PointGroup::C2v},
    {Name::TrigonalPyramidal, PointGroup::C3v},
    {Name::SquarePyramidal, PointGroup::C4v},
    {Name::TrigonalBiPyramidal, PointGroup::D3h},
    {Name::PentagonalPlanar, PointGroup::D5h},
    {Name::Octahedral, PointGroup::Oh},
    {Name::TrigonalPrismatic, PointGroup::D3h},
    {Name::PentagonalPyramidal, PointGroup::C5v},
    {Name::PentagonalBiPyramidal, PointGroup::D5h},
    {Name::SquareAntiPrismatic, PointGroup::D4d}
  };

  auto toCollection = [](const std::vector<Eigen::Vector3d>& vs) -> PositionCollection {
    const unsigned N = vs.size();
    PositionCollection positions(3, N + 1);
    for(unsigned i = 0; i < N; ++i) {
      positions.col(i) = vs.at(i);
    }

    // Add origin point explicitly to consideration
    positions.col(N) = Eigen::Vector3d::Zero(3);
    return positions;
  };

  analyze(toCollection(symmetryData().at(Name::PentagonalPyramidal).coordinates));

  //for(const Symmetry::Name symmetry : allNames) {
  //  std::cout << "Analyzing " << name(symmetry) << ":\n";
  //  analyze(toCollection(symmetryData().at(symmetry).coordinates));
  //}
}

BOOST_AUTO_TEST_CASE(PointGroupElements) {
  const std::vector<std::string> pointGroupStrings {
    "C1", "Ci", "Cs",
    "C2", "C3", "C4", "C5", "C6", "C7", "C8",
    "C2h", "C3h", "C4h", "C5h", "C6h", "C7h", "C8h",
    "C2v", "C3v", "C4v", "C5v", "C6v", "C7v", "C8v",
    "S4", "S6", "S8",
    "D2", "D3", "D4", "D5", "D6", "D7", "D8",
    "D2h", "D3h", "D4h", "D5h", "D6h", "D7h", "D8h",
    "D2d", "D3d", "D4d", "D5d", "D6d", "D7d", "D8d",
    "T", "Td", "Th",
    "O", "Oh",
    "I", "Ih",
    "Cinfv", "Dinfh"
  };

  auto writeXYZ = [](const std::string& filename, const PositionCollection& positions) {
    std::ofstream outfile(filename);
    const unsigned N = positions.cols();
    outfile << N << "\n\n";
    outfile << std::fixed << std::setprecision(10);
    for(unsigned i = 0; i < N; ++i) {
      outfile << std::left << std::setw(3) << "H";
      outfile << std::right
        << std::setw(16) << positions.col(i).x()
        << std::setw(16) << positions.col(i).y()
        << std::setw(16) << positions.col(i).z()
        << "\n";
    }
    outfile.close();
  };

  /* For each point group, create a point at unit x and z and apply all
   * transformations to each point
   */
  auto writePointGroup = [&](const PointGroup group) {
    const auto elements = minimization::symmetryElements(group);
    const auto groupings = minimization::npGroupings(elements);
    std::cout << pointGroupStrings.at(underlying(group)) << "\n";
    for(const auto& iterPair : groupings) {
      std::cout << "  np = " << iterPair.first << " along " << iterPair.second.probePoint.transpose() << " -> " << temple::stringify(iterPair.second.groups) << "\n";
    }
  };

  const PointGroup limit = PointGroup::Th;
  for(unsigned g = 0; g < underlying(limit); ++g) {
    writePointGroup(static_cast<PointGroup>(g));
  }
  writePointGroup(PointGroup::Oh);
}

BOOST_AUTO_TEST_CASE(Diophantine) {
  std::vector<unsigned> x;
  const std::vector<unsigned> a {4, 3, 2};
  const int b = 12;

  const std::vector<
    std::vector<unsigned>
  > expectedX {
    {0, 0, 6},
    {0, 2, 3},
    {0, 4, 0},
    {1, 0, 4},
    {1, 2, 1},
    {2, 0, 2},
    {3, 0, 0}
  };

  BOOST_REQUIRE(diophantine::first_solution(x, a, b));
  unsigned i = 0;
  do {
    BOOST_CHECK(x == expectedX.at(i));
    ++i;
  } while(diophantine::next_solution(x, a, b));
  BOOST_REQUIRE_EQUAL(i, expectedX.size());
  BOOST_REQUIRE(x == std::vector<unsigned> (3, 0));
}

BOOST_AUTO_TEST_CASE(Partitions) {
  for(unsigned i = 1; i < 4; ++i) {
    for(unsigned j = 2; j < 4; ++j) {
      Partitioner partitioner {i, j};
      do {
        BOOST_CHECK(Partitioner::isOrderedMapping(partitioner.map()));
      } while(partitioner.next_partition());
    }
  }

  // Test a few specific counts
  auto countPartitions = [](const unsigned S, const unsigned E) -> unsigned {
    Partitioner partitioner {S, E};
    unsigned partitions = 0;
    do {
      ++partitions;
    } while(partitioner.next_partition());
    return partitions;
  };

  BOOST_CHECK_EQUAL(countPartitions(1, 2), 1);
  BOOST_CHECK_EQUAL(countPartitions(2, 2), 3);
  BOOST_CHECK_EQUAL(countPartitions(2, 3), 10);
}
