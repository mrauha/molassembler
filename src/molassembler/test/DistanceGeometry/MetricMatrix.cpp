/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 */

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/filesystem.hpp"
#include "boost/test/unit_test.hpp"
#include "shapes/Data.h"
#include "Eigen/Eigenvalues"
#include "temple/Random.h"
#include "temple/Stringify.h"

#include "molassembler/Graph/InnerGraph.h"
#include "molassembler/DistanceGeometry/DistanceBoundsMatrix.h"
#include "molassembler/DistanceGeometry/ExplicitGraph.h"
#include "molassembler/DistanceGeometry/MetricMatrix.h"
#include "molassembler/DistanceGeometry/ConformerGeneration.h"
#include "molassembler/IO.h"
#include "molassembler/Options.h"

#include <iostream>
#include <random>

using namespace Scine;
using namespace molassembler;
using namespace DistanceGeometry;

Eigen::MatrixXd reorder(
  const Eigen::MatrixXd& sourceMatrix,
  const std::vector<unsigned>& reorderSequence
) {
  Eigen::MatrixXd retMatrix(reorderSequence.size(), reorderSequence.size());

  /* e.g. reorderSequence is {4, 2, 1, 3}
   *  and input is
   *
   *     1  2  3  4          4  2  1  3
   *
   * 1   -  1  2  3      4   -  5  3  6
   * 2   -  -  4  5  ->  2   -  -  1  4
   * 3   -  -  -  6  ->  1   -  -  -  2
   * 4   -  -  -  -      3   -  -  -  -
   *
   * reverse:
   *
   *     1  2  3  4          3  2  4  1
   *
   * 1   -  5  3  6      3   -  1  2  3
   * 2   -  -  1  4  ->  2   -  -  4  5
   * 3   -  -  -  2  ->  4   -  -  -  6
   * 4   -  -  -  -      1   -  -  -  -
   */


  for(unsigned i = 0; i < reorderSequence.size(); i++) {
    // Diagonal
    retMatrix(i, i) = sourceMatrix(
      reorderSequence[i],
      reorderSequence[i]
    );

    // Triangles
    for(unsigned j = i + 1; j < reorderSequence.size(); j++) {
      // Upper
      retMatrix(i, j) = sourceMatrix(
        std::min(
          reorderSequence[i],
          reorderSequence[j]
        ),
        std::max(
          reorderSequence[i],
          reorderSequence[j]
        )
      );

      // Lower
      retMatrix(j, i) = sourceMatrix(
        std::max(
          reorderSequence[i],
          reorderSequence[j]
        ),
        std::min(
          reorderSequence[i],
          reorderSequence[j]
        )
      );
    }
  }

  return retMatrix;
}

std::vector<unsigned> inverseReorderSequence(
  const std::vector<unsigned>& reorderSequence
) {
  /* reorder:
   *
   * 1 -> 4
   * 2 -> 2
   * 3 -> 1
   * 4 -> 3
   *
   * inverse:
   *
   * 1 -> 3 (what position 1 was at in the reorderSequence)
   * 2 -> 2 (...)
   * 3 -> 4
   * 4 -> 1
   */
  std::vector<unsigned> returnSequence (reorderSequence.size());

  for(unsigned i = 0; i < reorderSequence.size(); i++) {
    auto findIter = std::find(
      reorderSequence.begin(),
      reorderSequence.end(),
      i
    );
    if(findIter == reorderSequence.end()) {
      throw std::logic_error("Could not find i in reorderSequence!");
    }

    returnSequence[i] = findIter - reorderSequence.begin();
  }

  return returnSequence;
}

std::vector<unsigned> randomReorderingSequence(const unsigned length) {
  std::vector<unsigned> reorderSequence (length);

  std::iota(
    reorderSequence.begin(),
    reorderSequence.end(),
    0
  );

  temple::random::shuffle(reorderSequence, randomnessEngine());

  return reorderSequence;
}

BOOST_AUTO_TEST_CASE( reorderingWorks ) {
  bool allPassed = true;
  const unsigned N = 10;

  for(unsigned i = 0; i < 100; i++) {
    const Eigen::MatrixXd testMatrix = Eigen::MatrixXd::Random(N, N);

    auto reorderingOrder = randomReorderingSequence(N);

    auto result = reorder(
      reorder(
        testMatrix,
        reorderingOrder
      ),
      inverseReorderSequence(
        reorderingOrder
      )
    );

    if(testMatrix != result) {
      allPassed=false;

      std::cout << "Reordering reversibility failed! Original:\n"
        << testMatrix << std::endl
        << "Reordering sequence: " << temple::stringify(reorderingOrder) << std::endl
        << "Unreordering sequence: " << temple::stringify(inverseReorderSequence(reorderingOrder))
        << "Computed result: " << result << std::endl;

      break;
    }
  }

  BOOST_CHECK(allPassed);
}

BOOST_AUTO_TEST_CASE( constructionIsInvariantUnderOrderingSwap ) {
  for(
    const boost::filesystem::path& currentFilePath :
    boost::filesystem::recursive_directory_iterator("ez_stereocenters")
  ) {
    auto molecule = IO::read(currentFilePath.string());

    auto DGData = DistanceGeometry::gatherDGInformation(molecule, DistanceGeometry::Configuration {}, randomnessEngine());

    DistanceBoundsMatrix distanceBounds {
      molecule.graph().inner(),
      DGData.bounds
    };

    // choose a random reordering
    auto reorderSequence = randomReorderingSequence(
      molecule.graph().N()
    );

    // get a distances matrix from the bounds
    auto distancesMatrixResult = distanceBounds.makeDistanceMatrix(randomnessEngine());
    if(!distancesMatrixResult) {
      BOOST_FAIL(distancesMatrixResult.error().message());
    }
    auto distancesMatrix = distancesMatrixResult.value();

    // reorder the distance Bounds matrix
    auto reorderedDM = reorder(distancesMatrix, reorderSequence);

    // generate a metric matrix for both
    auto originalMetric = MetricMatrix(std::move(distancesMatrix));
    auto reorderedMetric = MetricMatrix(std::move(reorderedDM));

    /* Metric Matrix of original distances has to be identical to
     * un-reordered metric matrix of reordered distances
     */
    auto revert = reorder(
      reorderedMetric.access(),
      inverseReorderSequence(reorderSequence)
    );

    /* Since the metric matrix doesn't care about the top triangle of the
     * matrix, we have to take care of the random elements there here for
     * comparability.
     */
    revert.template triangularView<Eigen::Upper>().setZero();
    Eigen::MatrixXd originalMetricUnderlying = originalMetric.access();
    originalMetricUnderlying.template triangularView<Eigen::Upper>().setZero();

    if(!originalMetricUnderlying.isApprox(revert, 1e-7)) {
      std::cout << "Failed reordering test for "
        << currentFilePath.string() << ":" << std::endl
        << "Metric matrix from original distances matrix: " << std::endl
        << originalMetric.access() << std::endl
        << "un-reordered Metric matrix from reordered:"
        << std::endl << revert << std::endl;
      BOOST_FAIL("Reordering test fails!");
      break;
    }
  }
}

BOOST_AUTO_TEST_CASE( explicitFromLecture ) {
  // From Algorithms & Programming in C++ lecture
  Eigen::MatrixXd exactDistanceMatrix (4, 4);
  // No need to enter lower triangle
  exactDistanceMatrix << 0, 1, sqrt(2),       1,
                         0, 0,       1, sqrt(2),
                         0, 0,       0,       1,
                         0, 0,       0,       0;

  auto metric = MetricMatrix(exactDistanceMatrix);

  Eigen::MatrixXd expectedMetricMatrix (4, 4);
  expectedMetricMatrix <<  0.5,    0,    0,    0,
                             0,  0.5,    0,    0,
                          -0.5,    0,  0.5,    0,
                             0, -0.5,    0,  0.5;

  /* Copy out the metric matrix, zeroing out the upper strict triangle, since
   * it is uninitialized and unused by embedding.
   */
  Eigen::MatrixXd compareMatrix = metric.access();
  compareMatrix.triangularView<Eigen::StrictlyUpper>().setZero();

  BOOST_CHECK_MESSAGE(
    compareMatrix.isApprox(
      expectedMetricMatrix,
      1e-7
    ),
    "Do not get expected metric matrix from explicit example from lecture. Expect \n"
    << expectedMetricMatrix << "\ngot " << metric.access() << " instead.\n"
  );
}
