/*!@file
 * @copyright This code is licensed under the 3-clause BSD license.
 *   Copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt for details.
 */

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/filesystem.hpp"
#include "boost/test/unit_test.hpp"

#include "Molassembler/DirectedConformerGenerator.h"
#include "Molassembler/Molecule.h"
#include "Molassembler/IO.h"

#include "Utils/Typenames.h"

#include "Molassembler/Temple/Invoke.h"
#include "Molassembler/Temple/Stringify.h"

#include <fstream>
#include <iomanip>
#include <iostream>

using namespace std::string_literals;
using namespace Scine::Molassembler;

BOOST_AUTO_TEST_CASE(DirectedConformerGeneration, *boost::unit_test::label("DG")) {
  std::vector<
    std::tuple<std::string, unsigned, unsigned>
  > testCases {
    {"directed_conformer_generation/butane.mol", 1, 3},
    {"directed_conformer_generation/pentane.mol", 2, 9},
    {"directed_conformer_generation/caffeine.mol", 0, 0},
    {"isomorphisms/testosterone.mol", 1, 3},
  };

  auto executeTest = [](
    const std::string& filename,
    const unsigned numConsideredBonds,
    const unsigned idealEnsembleSize
  ) {
    auto mol = IO::read(filename);
    DirectedConformerGenerator generator(mol);

    BOOST_CHECK_MESSAGE(
      generator.bondList().size() == numConsideredBonds,
      "Bond list yielded by generator does not have expected size. Expected "
      << numConsideredBonds << " for " << filename << ", got "
      << generator.bondList().size() << " instead."
    );

    BOOST_CHECK_MESSAGE(
      generator.idealEnsembleSize() == idealEnsembleSize,
      "Generator ideal ensemble size does not yield expected number of "
      "conformers. Expected " << idealEnsembleSize << " for " << filename
        << ", got " << generator.idealEnsembleSize() << " instead."
    );

    // If there are
    if(idealEnsembleSize == 0) {
      return;
    }

    // Make a strict configuration. 2000 steps should be enough, even for testosterone
    DistanceGeometry::Configuration configuration {};
    configuration.refinementStepLimit = 2000;

    /* Ensure we can make generate all conformers we have hypothesized exist */
    const unsigned maxTries = 5;
    while(generator.decisionListSetSize() != generator.idealEnsembleSize()) {
      auto newDecisionList = generator.generateNewDecisionList();

      bool pass = false;
      boost::optional<DirectedConformerGenerator::DecisionList> generatedDecisionsOption;
      for(unsigned attempt = 0; attempt < maxTries; ++attempt) {
        auto positionResult = generator.generateRandomConformation(newDecisionList, configuration);

        if(positionResult) {
          generatedDecisionsOption = generator.getDecisionList(
            positionResult.value(),
            BondStereopermutator::FittingMode::Nearest
          );
          pass = true;
          break;
        }

        std::cout << "Conformer generation failure: " << positionResult.error().message() << "\n";
      }

      BOOST_CHECK_MESSAGE(
        pass,
        "Could not generate " << filename << " conformer w/ decision list: "
          << Temple::stringify(newDecisionList) << " in " << maxTries << " attempts"
      );

      if(generatedDecisionsOption) {
        BOOST_CHECK_MESSAGE(
          newDecisionList == generatedDecisionsOption.value(),
          "Supposedly generated and reinterpreted decision lists do not match:\n"
          << Temple::condense(newDecisionList) << " (supposedly generated)\n"
          << Temple::condense(generatedDecisionsOption.value())
          << " (reinterpreted)\n"
        );
      }
    }
  };

  for(const auto& tup : testCases) {
    Temple::invoke(executeTest, tup);
  }
}
