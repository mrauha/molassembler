/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 *
 * This file reimplements Distance Geometry with more invasive debug structures
 * and data collection.
 */

#define BOOST_FILESYSTEM_NO_DEPRECATED

#include "boost/filesystem.hpp"
#include "boost/program_options.hpp"

#include "molassembler/DistanceGeometry/ConformerGeneration.h"
#include "molassembler/DistanceGeometry/EigenRefinement.h"
#include "molassembler/DistanceGeometry/ExplicitGraph.h"
#include "molassembler/DistanceGeometry/MetricMatrix.h"
#include "molassembler/DistanceGeometry/RefinementMeta.h"
#include "molassembler/DistanceGeometry/TetrangleSmoothing.h"
#include "molassembler/IO.h"
#include "molassembler/IO/SmilesParser.h"
#include "molassembler/Log.h"

#include "temple/Adaptors/Enumerate.h"
#include "temple/Functional.h"
#include "temple/Optimization/LBFGS.h"
#include "temple/constexpr/Numeric.h"

#include <fstream>
#include <iomanip>

namespace Scine {
namespace molassembler {
namespace DistanceGeometry {
namespace detail {

template<typename EigenRefinementType>
struct InversionOrIterLimitStop {
  const unsigned iterLimit;
  const EigenRefinementType& refinementFunctorReference;


  using VectorType = typename EigenRefinementType::VectorType;
  using FloatType = typename EigenRefinementType::FloatingPointType;

  InversionOrIterLimitStop(
    const unsigned passIter,
    const EigenRefinementType& functor
  ) : iterLimit(passIter),
      refinementFunctorReference(functor)
  {}

  template<typename StepValues>
  bool shouldContinue(unsigned iteration, const StepValues& /* step */) {
    return (
      iteration < iterLimit
      && refinementFunctorReference.proportionChiralConstraintsCorrectSign < 1.0
    );
  }
};

template<typename FloatType>
struct GradientOrIterLimitStop {
  using VectorType = Eigen::Matrix<FloatType, Eigen::Dynamic, 1>;

  template<typename StepValues>
  bool shouldContinue(unsigned iteration, const StepValues& step) {
    return (
      iteration < iterLimit
      && step.gradients.current.template cast<double>().norm() > gradNorm
    );
  }

  unsigned iterLimit = 10000;
  double gradNorm = 1e-5;
};

} // namespace detail

MoleculeDGInformation gatherDGInformation(
  const Molecule& molecule,
  const Configuration& configuration,
  std::string& spatialModelGraphvizString,
  bool applyTetrangleSmoothing
) {
  // Generate a spatial model from the molecular graph and stereopermutators
  SpatialModel spatialModel {molecule, configuration};
  spatialModelGraphvizString = spatialModel.dumpGraphviz();

  // Extract gathered data
  MoleculeDGInformation data;
  data.bounds = spatialModel.makePairwiseBounds();
  data.chiralConstraints = spatialModel.getChiralConstraints();
  data.dihedralConstraints = spatialModel.getDihedralConstraints();

  if(applyTetrangleSmoothing) {
    /* Add implicit lower and upper bounds */
    const AtomIndex N = molecule.graph().N();
    for(AtomIndex i = 0; i < N; ++i) {
      for(AtomIndex j = i + 1; j < N; ++j) {
        double& lower = data.bounds(j, i);
        double& upper = data.bounds(i, j);

        if(lower == 0.0 && upper == 0.0) {
          double vdwLowerBound = (
            AtomInfo::vdwRadius(molecule.graph().elementType(i))
            + AtomInfo::vdwRadius(molecule.graph().elementType(j))
          );

          lower = vdwLowerBound;
          upper = 100.0;
        }
      }
    }

    unsigned iterations = tetrangleSmooth(data.bounds);
    std::cout << "Applied " << iterations << " iterations of tetrangle smoothing\n";
  }

  return data;
}

/*! @brief Logging, not throwing mostly identical implementation to run()
 *
 * A logging, not throwing, mostly identical implementation of
 * runDistanceGeometry that returns detailed intermediate data from
 * refinements, while run returns only the final conformers, which may
 * also be translated and rotated to satisfy fixed position constraints.
 */
std::list<RefinementData> debugRefinement(
  const Molecule& molecule,
  unsigned numConformers,
  const Configuration& configuration,
  bool applyTetrangleSmoothing
) {
  if(molecule.stereopermutators().hasZeroAssignmentStereopermutators()) {
    Log::log(Log::Level::Warning)
      << "This molecule has stereopermutators with zero valid permutations!"
      << std::endl;
  }

  SpatialModel::checkFixedPositionsPreconditions(molecule, configuration);

  std::list<RefinementData> refinementList;

  /* In case the molecule has unassigned stereopermutators that are not trivially
   * assignable (u/1 -> 0/1), random assignments have to be made prior to calling
   * gatherDGInformation (which creates the DistanceBoundsMatrix via the
   * SpatialModel, which expects all stereopermutators to be assigned).
   * Accordingly, gatherDGInformation has to be repeated in those cases, while
   * it is necessary only once in the other
   */

  /* There is also some degree of doubt about the relative frequencies of
   * assignments in stereopermutation. I should get on that too.
   */

  bool regenerateEachStep = molecule.stereopermutators().hasUnassignedStereopermutators();

  MoleculeDGInformation DGData;
  std::string spatialModelGraphviz;

  if(!regenerateEachStep) { // Collect once, keep all the time
    DGData = gatherDGInformation(
      molecule,
      configuration,
      spatialModelGraphviz,
      applyTetrangleSmoothing
    );
  }

  unsigned failures = 0;
  for(
    unsigned currentStructureNumber = 0;
    // Failed optimizations do not count towards successful completion
    currentStructureNumber < numConformers;
    ++currentStructureNumber
  ) {
    if(regenerateEachStep) {
      auto moleculeCopy = detail::narrow(molecule, randomnessEngine());

      if(moleculeCopy.stereopermutators().hasZeroAssignmentStereopermutators()) {
        Log::log(Log::Level::Warning)
          << "After setting stereopermutators at random, this molecule has "
          << "stereopermutators with zero valid permutations!"
          << std::endl;
      }

      // Fetch the DG data from the molecule with no unassigned stereopermutators
      DGData = gatherDGInformation(
        moleculeCopy,
        configuration,
        spatialModelGraphviz,
        applyTetrangleSmoothing
      );
    }

    std::list<RefinementStepData> refinementSteps;

    ExplicitGraph explicitGraph {
      molecule.graph().inner(),
      DGData.bounds
    };

    auto distanceBoundsResult = explicitGraph.makeDistanceBounds();
    if(!distanceBoundsResult) {
      Log::log(Log::Level::Warning) << "Failure in distance bounds matrix construction: "
        << distanceBoundsResult.error().message() << "\n";
      failures += 1;

      if(regenerateEachStep) {
        auto moleculeCopy = detail::narrow(molecule, randomnessEngine());

        SpatialModel model {moleculeCopy, configuration};
        model.writeGraphviz("DG-failure-spatial-model-" + std::to_string(currentStructureNumber) + ".dot");
      } else {
        SpatialModel model {molecule, configuration};
        model.writeGraphviz("DG-failure-spatial-model-" + std::to_string(currentStructureNumber) + ".dot");
      }

      continue;
    }

    DistanceBoundsMatrix distanceBounds {std::move(distanceBoundsResult.value())};

    /* No need to smooth the distance bounds, ExplicitGraph creates it
     * so that the triangle inequalities are fulfilled
     */

    assert(distanceBounds.boundInconsistencies() == 0);

    auto distanceMatrixResult = explicitGraph.makeDistanceMatrix(
      randomnessEngine(),
      configuration.partiality
    );
    if(!distanceMatrixResult) {
      Log::log(Log::Level::Warning) << "Failure in distance matrix construction.\n";
      failures += 1;
    }

    // Make a metric matrix from a generated distances matrix
    MetricMatrix metric(
      std::move(distanceMatrixResult.value())
    );

    // Get a position matrix by embedding the metric matrix
    auto embeddedPositions = metric.embed();

    /* Refinement problem compile-time settings
     * - Dimensionality four is needed to ensure chiral constraints invert
     *   nicely
     * - FloatType double is helpful for refinement stability, and float
     *   doesn't affect speed
     * - Using the alternative SIMD implementations of the refinement problems
     *   hardly affects speed at all, in fact, it commonly worsens it.
     */
    constexpr unsigned dimensionality = 4;
    using FloatType = double;
    constexpr bool SIMD = false;

    using FullRefinementType = EigenRefinementProblem<dimensionality, FloatType, SIMD>;
    using VectorType = typename FullRefinementType::VectorType;

    // Vectorize positions
    VectorType transformedPositions = Eigen::Map<Eigen::VectorXd>(
      embeddedPositions.data(),
      embeddedPositions.cols() * embeddedPositions.rows()
    ).template cast<FloatType>().eval();

    const unsigned N = transformedPositions.size() / dimensionality;

    const auto squaredBounds = static_cast<Eigen::MatrixXd>(
      distanceBounds.access().cwiseProduct(distanceBounds.access())
    );

    FullRefinementType refinementFunctor {
      squaredBounds,
      DGData.chiralConstraints,
      DGData.dihedralConstraints
    };

    /* If a count of chiral constraints reveals that more than half are
     * incorrect, we can invert the structure (by multiplying e.g. all y
     * coordinates with -1) and then have more than half of chirality
     * constraints correct! In the count, chiral constraints with a target
     * value of zero are not considered (this would skew the count as those
     * chiral constraints should not have to pass an energetic maximum to
     * converge properly as opposed to tetrahedra with volume).
     */
    double initiallyCorrectChiralConstraints = refinementFunctor.calculateProportionChiralConstraintsCorrectSign(transformedPositions);
    if(initiallyCorrectChiralConstraints < 0.5) {
      // Invert y coordinates
      for(unsigned i = 0; i < N; ++i) {
        transformedPositions(dimensionality * i + 1) *= -1;
      }

      initiallyCorrectChiralConstraints = 1 - initiallyCorrectChiralConstraints;
    }

    struct Observer {
      const Molecule& mol;
      FullRefinementType& functor;
      std::list<RefinementStepData>& steps;

      Observer(const Molecule& passMolecule, FullRefinementType& passFunctor, std::list<RefinementStepData>& passSteps)
        : mol(passMolecule), functor(passFunctor), steps(passSteps) {}

      using VectorType = typename FullRefinementType::VectorType;

      void operator() (const VectorType& positions) {
        FloatType distanceError = 0, chiralError = 0,
                  dihedralError = 0, fourthDimensionError = 0;

        VectorType gradient;
        gradient.resize(positions.size());
        gradient.setZero();

        // Call functions individually to get separate error values
        functor.distanceContributions(positions, distanceError, gradient);
        functor.chiralContributions(positions, chiralError, gradient);
        functor.dihedralContributions(positions, dihedralError, gradient);
        functor.fourthDimensionContributions(positions, fourthDimensionError, gradient);

        steps.emplace_back(
          positions.template cast<double>().eval(),
          distanceError,
          chiralError,
          dihedralError,
          fourthDimensionError,
          gradient.template cast<double>().eval(),
          functor.proportionChiralConstraintsCorrectSign,
          functor.compressFourthDimension
        );
      }
    };

    Observer observer(molecule, refinementFunctor, refinementSteps);

    /* Our embedded coordinates are (dimensionality) dimensional. Now we want
     * to make sure that all chiral constraints are correct, allowing the
     * structure to expand into the fourth spatial dimension if necessary to
     * allow inversion.
     *
     * This stage of refinement is only needed if not all chiral constraints
     * are already correct (or there are none).
     */
    unsigned firstStageIterations = 0;
    if(initiallyCorrectChiralConstraints < 1.0) {
      detail::InversionOrIterLimitStop<FullRefinementType> inversionChecker {
        configuration.refinementStepLimit,
        refinementFunctor
      };

      temple::LBFGS<FloatType, 32> optimizer;

      try {
        auto result = optimizer.minimize(
          transformedPositions,
          refinementFunctor,
          inversionChecker,
          observer
        );
        firstStageIterations = result.iterations;
      } catch(std::runtime_error& e) {
        Log::log(Log::Level::Warning)
          << "Non-finite contributions to dihedral error function gradient.\n";
        failures += 1;
        continue;
      }

      // Handle inversion failure (hit step limit)
      if(
        firstStageIterations >= configuration.refinementStepLimit
        || refinementFunctor.proportionChiralConstraintsCorrectSign < 1.0
      ) {
        Log::log(Log::Level::Warning)
          << "[" << currentStructureNumber << "]: "
          << "First stage of refinement fails. Loosening factor was "
          << configuration.spatialModelLoosening
          <<  "\n";
        failures += 1;
        continue; // this triggers a new structure to be generated
      }
    }

    /* Set up the second stage of refinement where we compress out the fourth
     * dimension that we allowed expansion into to invert the chiralities.
     */
    refinementFunctor.compressFourthDimension = true;


    unsigned secondStageIterations = 0;
    detail::GradientOrIterLimitStop<FloatType> gradientChecker;
    gradientChecker.gradNorm = 1e-3;
    gradientChecker.iterLimit = configuration.refinementStepLimit - firstStageIterations;

    try {
      temple::LBFGS<FloatType, 32> optimizer;

      auto result = optimizer.minimize(
        transformedPositions,
        refinementFunctor,
        gradientChecker,
        observer
      );
      secondStageIterations = result.iterations;
    } catch(std::out_of_range& e) {
      Log::log(Log::Level::Warning)
        << "Non-finite contributions to dihedral error function gradient.\n";
      failures += 1;
      continue;
    }

    if(secondStageIterations >= gradientChecker.iterLimit) {
        Log::log(Log::Level::Warning)
          << "[" << currentStructureNumber << "]: "
          << "Second stage of refinement fails!\n";
        failures += 1;

        // Collect refinement data
        RefinementData refinementData;
        refinementData.steps = std::move(refinementSteps);
        refinementData.constraints = DGData.chiralConstraints;
        refinementData.looseningFactor = configuration.spatialModelLoosening;
        refinementData.isFailure = true;
        refinementData.spatialModelGraphviz = spatialModelGraphviz;

        refinementList.push_back(
          std::move(refinementData)
        );

        if(Log::particulars.count(Log::Particulars::DGFinalErrorContributions) > 0) {
          explainFinalContributions(
            refinementFunctor,
            distanceBounds,
            transformedPositions
          );
        }

        continue; // this triggers a new structure to be generated
    }

    /* Add dihedral terms and refine again */
    unsigned thirdStageIterations = 0;
    gradientChecker = detail::GradientOrIterLimitStop<FloatType> {};
    gradientChecker.gradNorm = 1e-3;
    gradientChecker.iterLimit = (
      configuration.refinementStepLimit
      - firstStageIterations
      - secondStageIterations
    );

    refinementFunctor.dihedralTerms = true;

    try {
      temple::LBFGS<FloatType, 32> optimizer;

      auto result = optimizer.minimize(
        transformedPositions,
        refinementFunctor,
        gradientChecker,
        observer
      );
      thirdStageIterations = result.iterations;
    } catch(std::out_of_range& e) {
      Log::log(Log::Level::Warning)
        << "Non-finite contributions to dihedral error function gradient.\n";
      failures += 1;
      continue;
    }

    bool reachedMaxIterations = thirdStageIterations >= gradientChecker.iterLimit;
    bool notAllChiralitiesCorrect = refinementFunctor.proportionChiralConstraintsCorrectSign < 1;
    bool structureAcceptable = finalStructureAcceptable(
      refinementFunctor,
      distanceBounds,
      transformedPositions
    );

    if(Log::particulars.count(Log::Particulars::DGFinalErrorContributions) > 0) {
      explainFinalContributions(
        refinementFunctor,
        distanceBounds,
        transformedPositions
      );
    }

    RefinementData refinementData;
    refinementData.steps = std::move(refinementSteps);
    refinementData.constraints = DGData.chiralConstraints;
    refinementData.looseningFactor = configuration.spatialModelLoosening;
    refinementData.isFailure = (reachedMaxIterations || notAllChiralitiesCorrect || !structureAcceptable);
    refinementData.spatialModelGraphviz = spatialModelGraphviz;

    refinementList.push_back(
      std::move(refinementData)
    );

    if(reachedMaxIterations || notAllChiralitiesCorrect || !structureAcceptable) {
      Log::log(Log::Level::Warning)
        << "[" << currentStructureNumber << "]: "
        << "Third stage of refinement fails. Loosening factor was "
        << configuration.spatialModelLoosening
        <<  "\n";
      if(reachedMaxIterations) {
        Log::log(Log::Level::Warning) << "- Reached max iterations.\n";
      }

      if(notAllChiralitiesCorrect) {
        Log::log(Log::Level::Warning) << "- Not all chiral constraints have the correct sign.\n";
      }

      if(!structureAcceptable) {
        Log::log(Log::Level::Warning) << "- The final structure is unacceptable.\n";
        if(Log::isSet(Log::Particulars::DGStructureAcceptanceFailures)) {
          explainAcceptanceFailure(
            refinementFunctor,
            distanceBounds,
            transformedPositions
          );
        }
      }

      failures += 1;
    }
  }

  return refinementList;
}


} // namespace DistanceGeometry
} // namespace molassembler
} // namespace Scine

using namespace std::string_literals;
using namespace Scine;
using namespace molassembler;

void writeProgressFile(
  const Molecule& mol,
  const std::string& baseFilename,
  const unsigned index,
  const Eigen::VectorXd& positions
) {
  const std::string filename = baseFilename + "-" + std::to_string(index) + ".mol";
  AngstromWrapper angstromWrapper = DistanceGeometry::detail::convertToAngstromWrapper(
    DistanceGeometry::detail::gather(positions)
  );
  IO::write(filename, mol, angstromWrapper);
}

void writeProgressFiles(
  const Molecule& mol,
  const std::string& baseFilename,
  const DistanceGeometry::RefinementData& refinementData
) {
  /* Write the progress file */
  std::string progressFilename = baseFilename + "-progress.csv"s;
  std::ofstream progressFile (progressFilename);

  progressFile << std::scientific;

  for(const auto& refinementStep : refinementData.steps) {
    progressFile
      << refinementStep.distanceError << ","
      << refinementStep.chiralError << ","
      << refinementStep.dihedralError << ","
      << refinementStep.fourthDimError << ","
      << refinementStep.gradient.norm() << ","
      << static_cast<unsigned>(refinementStep.compress) << ","
      << refinementStep.proportionCorrectChiralConstraints << "\n";
  }

  progressFile.close();

  const unsigned maxProgressFiles = 100;

  if(refinementData.steps.size() > maxProgressFiles) {
    // Determine 100 roughly equispaced conformations to write to POV files
    double stepLength = static_cast<double>(refinementData.steps.size()) / maxProgressFiles;
    auto listIter = refinementData.steps.begin();
    unsigned currentIndex = 0;
    for(unsigned i = 0; i < maxProgressFiles; ++i) {
      unsigned targetIndex = std::floor(i * stepLength);
      assert(targetIndex >= currentIndex && targetIndex < refinementData.steps.size());
      std::advance(listIter, targetIndex - currentIndex);
      currentIndex = targetIndex;

      writeProgressFile(
        mol,
        baseFilename,
        i,
        listIter->positions
      );
    }
  } else {
    for(const auto enumPair : temple::adaptors::enumerate(refinementData.steps)) {
      writeProgressFile(
        mol,
        baseFilename,
        enumPair.index,
        enumPair.value.positions
      );
    }
  }

  // Write the graphviz representation of that structure number's spatial model
  std::string graphvizFilename = baseFilename + "-spatial-model.dot"s;
  std::ofstream graphvizfile (graphvizFilename);
  graphvizfile << refinementData.spatialModelGraphviz;
  graphvizfile.close();
}

const std::string partialityChoices =
  "  0 - Four-Atom Metrization\n"
  "  1 - 10% Metrization\n"
  "  2 - All (default)\n";

int main(int argc, char* argv[]) {
/* Set program options from command-line arguments */
  // Defaults
  unsigned nStructures = 1;

  bool showFinalContributions = false;
  bool applyTetrangleSmoothing = false;

  // Set up option parsing
  boost::program_options::options_description options_description("Recognized options");
  options_description.add_options()
    ("help,h", "Produce help message")
    (
      "num_conformers,n",
      boost::program_options::value<unsigned>(),
      "Set number of structures to generate"
    )
    (
      "from_file,f",
      boost::program_options::value<std::string>(),
      "Read molecule to generate from file"
    )
    (
      "line_notation,l",
      boost::program_options::value<std::string>(),
      "Generate molecule from passed SMILES string"
    )
    (
      "partiality,p",
      boost::program_options::value<unsigned>(),
      "Set metrization partiality option (Default: full)"
    )
    (
      "steps,s",
      boost::program_options::value<unsigned>(),
      "Alter the maximum number of refinement steps (Default: 10'000)"
    )
    (
      "contributions,c",
      boost::program_options::bool_switch(&showFinalContributions),
      "Show the final contributions to the refinement error functions"
    )
    (
      "tetrangle,t",
      boost::program_options::bool_switch(&applyTetrangleSmoothing),
      "Apply tetrangle smoothing once, prior to distance matrix generation"
    )
  ;

  // Parse
  boost::program_options::variables_map options_variables_map;
  boost::program_options::store(
    boost::program_options::command_line_parser(argc, argv).
    options(options_description).
    style(
      boost::program_options::command_line_style::unix_style
      | boost::program_options::command_line_style::allow_long_disguise
    ).run(),
    options_variables_map
  );
  boost::program_options::notify(options_variables_map);

  // Manage the results
  if(options_variables_map.count("help") > 0) {
    std::cout << options_description << std::endl;
    return 0;
  }

  if(options_variables_map.count("num_conformers") > 0) {
    unsigned argN = options_variables_map["num_conformers"].as<unsigned>();
    if(argN == 0) {
      std::cout << "Specified to generate zero structures. Exiting."
        << std::endl;
      return 0;
    }

    nStructures = argN;
  }

  DistanceGeometry::Partiality metrizationOption = DistanceGeometry::Partiality::All;
  if(options_variables_map.count("partiality") > 0) {
    unsigned index =  options_variables_map["partiality"].as<unsigned>();

    if(index > 2) {
      std::cout << "Specified metrization option is out of bounds. Valid choices are:\n"
        << partialityChoices;
      return 0;
    }

    metrizationOption = static_cast<DistanceGeometry::Partiality>(index);
  }

  Log::particulars.insert(Log::Particulars::DGStructureAcceptanceFailures);

  if(showFinalContributions) {
    Log::particulars.insert(Log::Particulars::DGFinalErrorContributions);
  }

  unsigned nSteps = 10000;
  if(options_variables_map.count("steps") > 0) {
    nSteps = options_variables_map["steps"].as<unsigned>();
  }

/* Generating work */
  std::string baseName;
  Molecule mol;

  // Generate from file
  if(options_variables_map.count("from_file") == 1) {
    auto filename = options_variables_map["from_file"].as<std::string>();

    if(!boost::filesystem::exists(filename)) {
      std::cout << "The specified file could not be found!\n";
      return 1;
    }

    mol = IO::read(filename);

    boost::filesystem::path filepath {filename};
    baseName = filepath.stem().string();
  } else if(options_variables_map.count("line_notation") == 1) {
    mol = IO::experimental::parseSmilesSingleMolecule(
      options_variables_map["line_notation"].as<std::string>()
    );
    baseName = "smiles";

    std::cout << mol << "\n";
  } else {
    std::cout << "No molecule input specified!\n";
    return 1;
  }

  std::ofstream graphFile(baseName +  "-graph.dot");
  graphFile << mol.dumpGraphviz();
  graphFile.close();

  DistanceGeometry::Configuration DGConfiguration;
  DGConfiguration.partiality = metrizationOption;
  DGConfiguration.refinementStepLimit = nSteps;

#ifndef NDEBUG
  auto debugData = DistanceGeometry::debugRefinement(
    mol,
    nStructures,
    DGConfiguration,
    applyTetrangleSmoothing
  );

  for(const auto& enumPair : temple::adaptors::enumerate(debugData)) {
    const auto& structNum = enumPair.index;
    const auto& refinementData = enumPair.value;

    std::string structBaseName = baseName + "-"s + std::to_string(structNum);

    writeProgressFiles(
      mol,
      structBaseName,
      refinementData
    );

    IO::write(
      structBaseName + "-last.mol"s,
      mol,
      DistanceGeometry::detail::convertToAngstromWrapper(
        DistanceGeometry::detail::gather(refinementData.steps.back().positions)
      )
    );
  }

  auto failures = temple::sum(
    temple::map(
      debugData,
      [](const auto& refinementData) -> unsigned {
        return static_cast<unsigned>(refinementData.isFailure);
      }
    )
  );

  if(failures > 0) {
    std::cout << "WARNING: " << failures << " refinements failed.\n";
  }
#else
  auto conformers = DistanceGeometry::run(
    mol,
    nStructures,
    DGConfiguration,
    boost::none
  );

  unsigned i = 0;
  unsigned failures = 0;
  for(const auto& conformerResult : conformers) {
    if(conformerResult) {
      IO::write(
        baseName + "-"s + std::to_string(i) + "-last.mol"s,
        mol,
        conformerResult.value()
      );
    } else {
      std::cout << "Conformer " << i << " failed: " << conformerResult.error().message() << "\n";
      ++failures;
    }

    ++i;
  }

  std::cout << "WARNING: " << failures << " refinement(s) failed.\n";
#endif
}
