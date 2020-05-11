/*!@file
 * @copyright This code is licensed under the 3-clause BSD license.
 *   Copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt for details.
 */

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/test/unit_test.hpp"
#include "boost/filesystem.hpp"

#include "Molassembler/Descriptors.h"
#include "Molassembler/Molecule.h"
#include "Molassembler/IO.h"

#include <iostream>

using namespace Scine::Molassembler;

std::map<
  std::string,
  unsigned
> rotatableBondExpectations {
  {"Benzene", 0},
  {"Cyclobutadiene", 1},
  {"Cyclobutane", 1},
  {"Cyclohexane", 3},
  {"Cyclohexene", 3}, // 2.5 rounded up
  {"Cyclopentane", 2},
  {"Cyclopentene", 2}, // 8/5 rounded up
  {"Cyclopropane", 0},
  {"EEDifluorobutadiene", 1},
  {"Toluol", 1}
};

BOOST_AUTO_TEST_CASE(RotatableBondsDescriptorsExamples) {
  for(
    const boost::filesystem::path& currentFilePath :
    boost::filesystem::recursive_directory_iterator("descriptor_test_set")
  ) {
    auto molecule = IO::read(currentFilePath.string());

    std::string moleculeName = currentFilePath.stem().string();

    auto findIter = rotatableBondExpectations.find(moleculeName);

    BOOST_REQUIRE_MESSAGE(
      findIter != std::end(rotatableBondExpectations),
      "There is no test entry for the number of rotatable bonds of " << moleculeName
    );

    unsigned expected = findIter->second;
    unsigned result = numRotatableBonds(molecule);

    if(expected != result) {
      std::cout << "Interpreted molecule stereocenters for "
        << moleculeName << ": " << molecule << "\n";
    }

    BOOST_CHECK_MESSAGE(
      expected == result,
      "Number of rotatable bonds does not match expectation for "
        << moleculeName << ": Expected " << expected << ", got " << result
        << " instead."
    );
  }
}