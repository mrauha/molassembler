#include "BoostTestingHeader.h"
#include "AdjacencyMatrix.h"

/*   Member listing
 * y 1 AdjacencyList constructor
 * y 2 getMatrixRef
 * y 3 altering operator ()
 * y 4 non-altering operator ()
 */

BOOST_AUTO_TEST_CASE( AdjacencyMatrix_all ) {
  using namespace MoleculeManip;
  EdgeList edges{
    Edge(0, 1, BondType::Single),
    Edge(1, 2, BondType::Single),
    Edge(1, 4, BondType::Single),
    Edge(2, 3, BondType::Single),
    Edge(3, 4, BondType::Single),
    Edge(4, 5, BondType::Single),
    Edge(5, 6, BondType::Single),
    Edge(5, 7, BondType::Single)
  };

  /* 1 */
  // use uniform initialization syntax to avoid most vexing parse
  AdjacencyMatrix testInstance {
    AdjacencyList {
      edges
    }
  };

  BOOST_CHECK(testInstance.N == 8);

  for(const auto& edge: edges) {
    /* 4 */
    BOOST_CHECK(
      testInstance(
        edge.j, // since order shouldn't matter
        edge.i
      ) // the positions are boolean already
    );
  }

  /* 3 */
  testInstance(5, 2) = true;

  /* 2 */
  BOOST_CHECK(testInstance.getMatrixRef()(2, 5));
  // This is faulty, we cannot say anything about the state of the lower matrix
  // BOOST_CHECK(testInstance.getMatrixRef()(5, 2) == 0);



}
