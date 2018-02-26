#define BOOST_TEST_MODULE ExplicitGraphTestModule
#define BOOST_TEST_DYN_LINK
#include "boost/test/unit_test.hpp"
#define BOOST_FILESYSTEM_NO_DEPRECATED
#include "boost/filesystem.hpp"

#include "DistanceGeometry/ExplicitGraph.h"
#include "DistanceGeometry/MoleculeSpatialModel.h"

#include "boost/graph/graph_concepts.hpp"
#include "IO.h"

#include <iostream>
#include <iomanip>

inline std::ostream& nl(std::ostream& os) {
  os << '\n';
  return os;
}

template<typename UnsignedType>
UnsignedType left(UnsignedType a) {
  return 2 * a;
}

template<typename UnsignedType>
UnsignedType right(UnsignedType a) {
  return 2 * a + 1;
}

BOOST_AUTO_TEST_CASE(nonVisualTests) {
  using namespace MoleculeManip;

  boost::filesystem::path filesPath("../tests/mol_files/stereocenter_detection_molecules");
  boost::filesystem::recursive_directory_iterator end;

  IO::MOLFileHandler molHandler;
  for(boost::filesystem::recursive_directory_iterator i(filesPath); i != end; i++) {
    const boost::filesystem::path currentFilePath = *i;

    Molecule molecule = molHandler.readSingle(
      currentFilePath.string()
    );

    using EG = DistanceGeometry::ExplicitGraph;

    DistanceGeometry::MoleculeSpatialModel spatialModel {
      molecule,
      DistanceGeometry::MoleculeSpatialModel::DistanceMethod::UFFLike
    };

    EG explicitGraph {
      molecule,
      spatialModel.makeBoundList()
    };

    auto spg = explicitGraph.getGraph();

    EG::VertexDescriptor N = boost::num_vertices(spg);

    for(EG::VertexDescriptor a = 0; a < N / 2; ++a) {
      BOOST_CHECK_MESSAGE(
        !boost::edge(2 * a, 2 * a + 1, spg).second,
        "Same-a edge exists for a = " << a
      );

      for(EG::VertexDescriptor b = 0; b < N / 2; ++b) {
        if(a == b) {
          continue;
        }

        // No right-to-left edges
        BOOST_CHECK_MESSAGE(
          !boost::edge(right(a), left(b), spg).second,
          "r(a) -> l(b) for a = " << a << ", b = " << b
        );

        BOOST_CHECK_MESSAGE(
          !boost::edge(right(b), left(a), spg).second,
          "r(b) -> l(a) for a = " << a << ", b = " << b
        );

        // If there is an edge from la to lb
        auto lalb = boost::edge(2 * a, 2 * b, spg);
        if(lalb.second) {
          auto lalbWeight = boost::get(boost::edge_weight, spg, lalb.first);

          auto lbra = boost::edge(2 * b, 2 * a + 1, spg);

          BOOST_CHECK_MESSAGE(
            lbra.second,
            "matching l(b) -> r(a) edge does not exist for a = " << a
              << ", b = " << b
          );

          auto lbraWeight = boost::get(boost::edge_weight, spg, lbra.first);

          auto larb = boost::edge(2 * a, 2 * b + 1, spg);

          BOOST_CHECK_MESSAGE(
            larb.second,
            "matching l(a) -> r(b) edge does not exist for a = " << a
              << ", b = " << b
          );

          auto larbWeight = boost::get(boost::edge_weight, spg, larb.first);

          BOOST_CHECK_MESSAGE(
            larbWeight == lbraWeight,
            "l(a) -> r(b) and l(b) -> r(a) edges do not have same weight for a = "
              << a << ", b = " << b << ": " << larbWeight << ", " << lbraWeight
          );

          BOOST_CHECK_MESSAGE(
            lalbWeight > std::fabs(larbWeight),
            "l(a) -> l(b) weight isn't greater than abs of l(a) -> r(b) weight for a = "
              << a << ", b = " << b
          );
        }
      }
    }

    auto edge_iterators = boost::edges(spg);
    auto iter = edge_iterators.first;
    auto end = edge_iterators.second;

    while(iter != end) {
      auto edgeDescriptor = *iter;

      auto boostSource = boost::source(edgeDescriptor, spg);
      auto boostTarget = boost::target(edgeDescriptor, spg);

      // No right-to-left edges
      BOOST_CHECK_MESSAGE(
        !(!EG::isLeft(boostSource) && EG::isLeft(boostTarget)),
        "Edge points from right to left! " << boostSource << " -> " << boostTarget
      );

      auto boostEdgeWeight = boost::get(boost::edge_weight, spg, edgeDescriptor);

      if(boostSource % 2 == boostTarget % 2) {
        // In-group edge

        // Reverse exists
        auto reverseEdge = boost::edge(boostTarget, boostSource, spg);
        BOOST_CHECK_MESSAGE(
          reverseEdge.second, 
          "Reverse edge does not exist for in-group edge " 
            << boostSource << " -> " << boostTarget
        );

        // Reverse has same edge weight
        BOOST_CHECK_MESSAGE(
          boost::get(boost::edge_weight, spg, reverseEdge.first) == boostEdgeWeight,
          "Reverse edge for " << boostSource << " -> " << boostTarget 
            << " does not have same edge weight"
        );
      }

      ++iter;
    }

    // Generated distances matrix must satisfy triangle inequalities
    auto distancesMatrixResult = explicitGraph.makeDistanceMatrix();
    BOOST_REQUIRE_MESSAGE(distancesMatrixResult, distancesMatrixResult.error().message());
    auto distancesMatrix = distancesMatrixResult.value();

    auto d = [&distancesMatrix](const AtomIndexType& i, const AtomIndexType& j) -> double {
      return distancesMatrix(
        std::min(i, j),
        std::max(i, j)
      );
    };

    bool passTriangleInequalities = true;
    unsigned matrN = distancesMatrix.cols();
    for(AtomIndexType i = 0; i < matrN; ++i) {
      for(AtomIndexType j = 0; j < matrN; ++j) {
        if(i == j) {
          continue;
        }

        for(AtomIndexType k = 0; k < matrN; ++k) {
          if(j == k) {
            continue;
          }

          if(d(i, k) > d(i, j) + d(j, k)) {
            passTriangleInequalities = false;
            std::cout << "The triangle inequality is falsified along i = " << i
              << ", j = " << j << ", k = " << k << "!" 
              << " d(i, k) = " << (d(i, k)) << " > " << (d(i, j) + d(j, k))
              << " = d(i, j) + d(j, k)"
              << nl;
          }
        }
      }
    }

    BOOST_CHECK_MESSAGE(
      passTriangleInequalities,
      "Generated distance matrix does not satisfy triangle inequalities!"
        << nl << distancesMatrix << nl
    );
  }
}