#ifndef LIB_INCLUDE_SYMMETRIES_H
#define LIB_INCLUDE_SYMMETRIES_H

/* If USE_ALTERNATE_TETRAHEDRA is defined, a reduced set of tetrahedra
 * is used to subdivide higher symmetries. This may provide less information 
 * about the geometry when used but should improve performance as fewer 
 * tetrahedron volumes must be calculated.
 */
//#define USE_ALTERNATE_TETRAHEDRA

/* If USE_CONSTEXPR_SQUARE_ANTIPRISMATIC_LOOKUP_TABLE is defined, a table of all
 * angles resulting from a predefined set of positions is generated and that
 * symmetry's angle function turns into what is essentially a lookup table.
 */
#define USE_CONSTEXPR_SQUARE_ANTIPRISMATIC_LOOKUP_TABLE

/* If USE_CONSTEXPR_TRANSITION_MAPPINGS is defined, a data structure containing
 * the best index mappings between symmetries is generated at compile-time.
 * Transitions are alterations of the symmetry primitives in three-dimensional
 * space which can involve loss of a ligand position, merely a rearrangement to
 * another spatial symmetry of the same size, or the gain of a ligand position.
 *
 * Defining this preprocessor variable is currently discouraged since the
 * compile-time cost introduced is excessive in comparison to the runtime cost.
 * Although the program is well-formed, GCC version 7.2.0 does not recognize it
 * as such due to compiler bugs. Circumventing the bugs with additional
 * measures increases the compilation cost to at least 32 GB of memory, at
 * which compilation is aborted and considered failed here. Clang 4.0.0 can
 * compile the full library of dynamic and constexpr algorithms and execute
 * them at compile-time to generate a subset of the complete data structure
 * (transitions up to and within symmetries of size 5) within roughly a minute,
 * but a full treatment of symmetries up to size 8 would take a minimum of 41
 * hours.
 *
 * Executing the same constexpr functions at runtime and comparing them with the
 * implementations directly intended for runtime use takes around 3 minutes.
 *
 * To the implementor, it is unclear what causes this enormous disparity in
 * execution time needed at compile time and run time, when the algorithms being
 * executed are clearly the same. In GCC, memoization of the arguments and
 * results of constexpr functions could be the cause of ballooning memory costs,
 * since nothing of the sort is incurred in Clang, which does not memoize calls.
 *
 * The constexpr algorithm laid bare here is the first such program of the
 * author, and thus the costs of the compile-time branching and instantiation
 * methods are most likely not optimal.
 *
 * See the Rule of Chiel, from Odin Holmes' "Type Based Metaprogramming is not
 * Dead" talk at C++Now 2017: https://www.youtube.com/watch?v=EtU4RDCCsiU&t=9m08
 *
 * Certainly, the constexpr algorithm requires many type instantiations since
 * array-like types are reinstantiated for any new maximum size estimate, heavy
 * use of SFINAE is made for compile-time if expressions, and there are a great
 * number of function templates around, which could explain a heavy cost of
 * compiling the functions, but there is no such cost. The cost comes from
 * executing the constexpr functions *at compile time* instead of at runtime.
 */
#define USE_CONSTEXPR_TRANSITION_MAPPINGS

#include "Eigen/Core"
#include "boost/optional.hpp"

#ifdef USE_CONSTEXPR_SQUARE_ANTIPRISMATIC_LOOKUP_TABLE
#include "AngleLookup.h"
#endif

#include "constexpr_magic/Math.h"
#include "constexpr_magic/TupleType.h"
#include "template_magic/Containers.h"

#include <map>
#include <vector>
#include <functional>
#include <algorithm>

/*! @file
 *
 * Central inclusion point of the library. Defines the main symmetry data and
 * all accessors. Symmetries are defined in a constexpr fashion and homogenized
 * into a single container at compile-time to allow for compile-time computation
 * without losing universal accessibility at run-time.
 */

/* TODO
 * - More docstrings
 * - Debug and Release builds
 * - Improve trigonal pyramidal coordinates definition to get 107.5 angle as a
 *   parameter.  Currently, the rotation angle choice of 111.5 works well, but
 *   completely arbitrary!
 * - Consider making constexpr calculation of all angles from coordinates into
 *   const lookup table
 * - Could replicate angle parametrization of coordinates with a constexpr
 *   matrix class and matrix * vector multiplication
 */

namespace Symmetry {

/* Typedefs */
//! The type to store symmetry rotations
using RotationsList = std::vector<
  std::vector<unsigned>
>;

/*!
 * All angle functions can be called with arbitrary (valid) parameters
 * without failing. Valid here means that a != b and less than the size of
 * the symmetry requested.
 *
 * They return angles in radians.
 */
using AngleFunctionType = std::function<
  double(const unsigned&, const unsigned&)
>;

/*! 
 * All symmetries have a guess implementation of what could work as the defined
 * tetrahedra. Have to use boost::none to signal to replace this position with 
 * the central atom as it is not part of the indexing scheme used here.
 *
 * In case all higher symmetries than trigonal pyramidal are representable 
 * without boost::none and that proves to work, then perhaps make an exception 
 * for it and treat all others without the optional. If that cannot be done, 
 * consider refactoring (changing the numbering scheme in some fashion that 
 * boost::none does not have to be used.
 */
using TetrahedronList = std::vector<
  std::array<
    boost::optional<unsigned>,
    4
  >
>;

using CoordinateList = std::vector<Eigen::Vector3d>;

//! Dynamic symmetry information data struct
struct SymmetryInformation {
  const std::string stringName;
  const unsigned size;
  const RotationsList rotations;
  const TetrahedronList tetrahedra;
  const CoordinateList coordinates;

  // Direct initialization
  SymmetryInformation(
    std::string stringName,
    unsigned size,
    RotationsList rotations,
    TetrahedronList tetrahedra,
    CoordinateList coordinates
  ) : stringName(stringName),
      size(size),
      rotations(rotations),
      tetrahedra(tetrahedra),
      coordinates(coordinates)
  {}
};

//! Enumeration of all contained symmetry names
enum class Name : unsigned {
  Linear, // 2
  Bent,
  TrigonalPlanar, // 3
  TrigonalPyramidal,
  TShaped,
  Tetrahedral, // 4
  SquarePlanar,
  Seesaw,
  SquarePyramidal, // 5
  TrigonalBiPyramidal,
  PentagonalPlanar,
  Octahedral, // 6
  TrigonalPrismatic,
  PentagonalPyramidal,
  PentagonalBiPyramidal, // 7
  SquareAntiPrismatic // 8
};

//! Total number of contained symmetries
constexpr unsigned nSymmetries = 16;

// Helper function to create all names vector
template<size_t ... Inds>
constexpr std::array<Name, nSymmetries> makeAllNames(
  std::index_sequence<Inds...>
) {
  return {{
    static_cast<Name>(Inds)...
  }};
}

//! A list of all the enum class values
constexpr std::array<Name, nSymmetries> allNames = makeAllNames(
  std::make_index_sequence<nSymmetries>()
);

//! A placeholder value for constexpr tetrahedra specification of origin
constexpr unsigned ORIGIN_PLACEHOLDER = std::numeric_limits<unsigned>::max();


/*! 
 * Namespace containing all symmetry data classes and some minor helper functions
 *
 * Each symmetry data class must have the following members, all of which must
 * be static constexpr (or static const in the exception of stringName):
 * - name (Symmetry::Name)
 * - size (unsigned)
 * - stringName (string)
 * - angleFunction ( double(const unsigned&, const unsigned&) )
 * - coordinates ( array<ConstexprMagic::Vector, N> ): N is size of symmetry,
 *   see above
 * - rotations ( array< array<unsigned, size>, R> ): R is however many
 *   rotations are needed to produce all superimposable rotations
 * - tetrahedra ( array< array<unsigned, 4>, T> ): T is however many tetrahedra
 *   are required to completely define the chirality
 */
namespace data {

struct Linear {
  /*
   *  0 – (_) – 1
   */
  static constexpr Symmetry::Name name = Symmetry::Name::Linear;
  static constexpr unsigned size = 2;
  static constexpr char stringName[] = "linear";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    return M_PI;
  }
  static constexpr std::array<ConstexprMagic::Vector, 2> coordinates {{
    { 1 , 0, 0 },
    { -1, 0, 0 }
  }};
  static constexpr std::array<
    std::array<unsigned, 2>,
    1
  > rotations {{
    {{1, 0}}
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    0
  > tetrahedra {{}};
};

struct Bent {
  /*
   *  1
   *   \
   *    (_) – 0
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::Bent;
  static constexpr unsigned size = 2;
  static constexpr char stringName[] = "bent";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    /* subject to a lot of variation, between 90 and 109 degrees pursuant to 
     * english wikipedia, using experimental data here to improve instances
     * of this geometry on e.g. O center would a big improvement to DG runs
     */
    if(a == b) {
      return 0;
    }

    return ConstexprMagic::Math::toRadians<double>(107); 
  }
  static constexpr std::array<ConstexprMagic::Vector, 2> coordinates {{
    {1., 0., 0.},
    {-0.292372, 0.956305, 0.}
  }};
  static constexpr std::array<
    std::array<unsigned, 2>,
    1
  > rotations {{
    {{1, 0}}
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    0
  > tetrahedra {{}};
};

struct TrigonalPlanar {
  /*
   *     0
   *     |
   *    (_)
   *   /   \
   *  1     2
   *
   * This is not quite ideal since the angles are thoroughly misrepresented, 
   * but all positions including the central atom are in one plane. The angles
   * are idealized as 120°.
   */
  static constexpr Symmetry::Name name = Symmetry::Name::TrigonalPlanar;
  static constexpr unsigned size = 3;
  static constexpr char stringName[] = "trigonal planar";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    return ConstexprMagic::Math::toRadians<double>(120);
  }
  static constexpr std::array<ConstexprMagic::Vector, 3> coordinates {{
      {1, 0, 0},
      {-0.5, 0.866025, 0},
      {-0.5, -0.866025, 0}
  }};
  static constexpr std::array<
    std::array<unsigned, 3>,
    2
  > rotations {{
    {{1, 2, 0}}, // C3
    {{0, 2, 1}} // C2
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    0
  > tetrahedra {{}};
};

struct TrigonalPyramidal {
  /*
   *     0
   *     |
   *    (_)
   *   /   \
   *  1     2
   *
   * This is not quite ideal since the angles are thoroughly misrepresented, 
   * but all positions including the central atom are in one plane. The angles
   * are idealized as 120°.
   */
  static constexpr Symmetry::Name name = Symmetry::Name::TrigonalPyramidal;
  static constexpr unsigned size = 3;
  static constexpr char stringName[] = "trigonal pyramidal";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    return ConstexprMagic::Math::toRadians<double>(107.5);
  }
  static constexpr std::array<ConstexprMagic::Vector, 3> coordinates {{
    {0, -0.366501, 0.930418},
    {0.805765, -0.366501, -0.465209},
    {-0.805765, -0.366501, -0.465209}
  }};
  static constexpr std::array<
    std::array<unsigned, 3>,
    1
  > rotations {{
    {{2, 0, 1}} // C3
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    1
  > tetrahedra {{
    {{ORIGIN_PLACEHOLDER, 0, 1, 2}}
  }};
};

struct TShaped {
  /*
   * 0 – (_) – 2
   *      |
   *      1
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::TShaped;
  static constexpr unsigned size = 3;
  static constexpr char stringName[] = "T-shaped";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    if((a + b) % 2 == 1) {
      return M_PI / 2;
    } 

    return M_PI;
  }
  static constexpr std::array<ConstexprMagic::Vector, 3> coordinates {{
    {-1, -0, -0},
    {0, 1, 0},
    {1, 0, 0},
  }};
  static constexpr std::array<
    std::array<unsigned, 3>,
    1
  > rotations {{
    {{2, 1, 0}} // C2
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    0
  > tetrahedra {{}};
};

struct Tetrahedral {
  /* 
   *    1
   *    |
   *   (0) (0 is on top, ( ) signifies the central atom  beneath it
   *  /   \
   * 2     3
   *
   * Remember Newman projections? This is sort of supposed to be that.
   *
   * Alternatively:
   *
   *    0
   *    |
   *   (_)   
   *  /  \ °3
   * 1    2 
   *
   * Where /, \ denote in front of plane bonds, ° a behind the plane bond.
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::Tetrahedral;
  static constexpr unsigned size = 4;
  static constexpr char stringName[] = "tetrahedral";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    return ConstexprMagic::Math::toRadians<double>(109.5);
  }
  static constexpr std::array<ConstexprMagic::Vector, 4> coordinates {{
    {0, 1, 0},
    {0, -0.333807, 0.942641},
    {0.816351, -0.333807, -0.471321},
    {-0.816351, -0.333807, -0.471321}
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    4
  > rotations {{
    {{0, 3, 1, 2}}, // C4, 1
    {{2, 1, 3, 0}}, // C4, 2
    {{3, 0, 2, 1}}, // C4, 3
    {{1, 2, 0, 3}}  // C4, 4
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    1
  > tetrahedra {{
    {{0, 1, 2, 3}}
  }};
};

struct SquarePlanar {
  /* 
   * 3   2
   *  \_/
   *  (_) <- central atom
   *  / \
   * 0   1
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::SquarePlanar;
  static constexpr unsigned size = 4;
  static constexpr char stringName[] = "square planar";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    if((a + b) % 2 == 1) {
      // this expression indicates cis
      return M_PI / 2;
    } 

    // leftover case is trans
    return M_PI;
  }
  static constexpr std::array<ConstexprMagic::Vector, 4> coordinates {{
    {1, 0, 0},
    {0, 1, 0},
    {-1, -0, -0},
    {-0, -1, -0}
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    3
  > rotations {{
    {{3, 0, 1, 2}}, // C4
    {{1, 0, 3, 2}}, // C2
    {{3, 2, 1, 0}}  // C2'
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    0
  > tetrahedra {{}};
};

struct Seesaw {
  /*
   * 0 – (_) – 3
   *     / :
   *    1   2
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::Seesaw;
  static constexpr unsigned size = 4;
  static constexpr char stringName[] = "seesaw";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    const auto& smaller = std::min(a, b);
    const auto& larger = std::max(a, b);
    if(smaller == 0 && larger == 3) {
      return M_PI;
    }

    if(smaller == 1 && larger == 2) {
      return ConstexprMagic::Math::toRadians<double>(120);
    }

    return M_PI / 2;
  }
  static constexpr std::array<ConstexprMagic::Vector, 4> coordinates {{
    {0, 1, 0},
    {1, 0, 0},
    {-0.5, 0, -0.866025},
    {-0, -1, -0}
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    1
  > rotations {{
    {{3, 2, 1, 0}} // C2
  }};

#ifdef USE_ALTERNATE_TETRAHEDRA
  static constexpr std::array<
    std::array<unsigned, 4>,
    1
  > tetrahedra {{
    {{0, 1, 2, 3}}
  }};
#else // Regular
  static constexpr std::array<
    std::array<unsigned, 4>,
    2
  > tetrahedra {{
    {{0, ORIGIN_PLACEHOLDER, 1, 2}},
    {{ORIGIN_PLACEHOLDER, 3, 1, 2}},
  }};
#endif
};

struct SquarePyramidal {
  /* 
   * 3   2
   *  \_/
   *  (4)   
   *  / \
   * 0   1
   *
   * Viewed from the top of the pyramid. The central atom is ( ), 5 is axial.
   *
   * Alternatively,
   *
   *    4
   * 3  |  2
   *  : | :    <- behind view plane
   *   (_)
   *  /   \    <- in front of view plane
   * 0     1
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::SquarePyramidal;
  static constexpr unsigned size = 5;
  static constexpr char stringName[] = "square pyramidal";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    if(a == 4 || b == 4) { // all bonds to axial ligand are 90°
      return M_PI / 2; 
    }

    if((a + b) % 2 == 0) { // 0 + 2 or 1 + 3 are trans
      return M_PI;
    }

    // rest are cis
    return M_PI / 2;
  }
  static constexpr std::array<ConstexprMagic::Vector, 5> coordinates {{
    {1, 0, 0},
    {0, 1, 0},
    {-1, -0, -0},
    {-0, -1, -0},
    {0, 0, 1}
  }};
  static constexpr std::array<
    std::array<unsigned, 5>,
    1
  > rotations {{
    {{3, 0, 1, 2, 4}} // C4
  }};

#ifdef USE_ALTERNATE_TETRAHEDRA
  static constexpr std::array<
    std::array<unsigned, 4>,
    2
  > tetrahedra {{
    {{0, 1, 4, 2}}, 
    {{0, 3, 2, 4}}
  }};
#else // Regular
  static constexpr std::array<
    std::array<unsigned, 4>,
    4
  > tetrahedra {{
    {{0, 1, 4, ORIGIN_PLACEHOLDER}},
    {{1, 2, 4, ORIGIN_PLACEHOLDER}},
    {{2, 3, 4, ORIGIN_PLACEHOLDER}},
    {{3, 0, 4, ORIGIN_PLACEHOLDER}}
  }};
#endif
};

struct TrigonalBiPyramidal {
  /* Viewed from the top of the pyramid. The central atom is ( ), 3 and 4 
   * are axial.
   *
   *     3
   *     |  2
   *     | :    <- behind view plane
   * 0--(_)
   *     | \    <- in front of view plane
   *     |  1
   *     4
   */
  static constexpr Symmetry::Name name = Symmetry::Name::TrigonalBiPyramidal;
  static constexpr unsigned size = 5;
  static constexpr char stringName[] = "trigonal bipyramidal";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    unsigned smaller = std::min(a, b), larger = std::max(a, b);
    if(larger < 3) {
      // -> smaller < 2, this means either 0,1 0,2 1,2 axial
      return ConstexprMagic::Math::toRadians<double>(120);
    } else if(larger == 3) {
      // -> smaller < 3, this means {1,2,3}, 3 
      return M_PI / 2;
    } else if(smaller < 3) {
      // now, larger must be 4 (process of elimination), so if a is not 3:
      return M_PI / 2;
    } else {
      // only case left: 3,4
      return M_PI;
    }
  }
  static constexpr std::array<ConstexprMagic::Vector, 5> coordinates {{
    {1, 0, 0},
    {-0.5, 0.866025, 0},
    {-0.5, -0.866025, 0},
    {0, 0, 1},
    {-0, -0, -1}
  }};
  static constexpr std::array<
    std::array<unsigned, 5>,
    4
  > rotations {{
    {{2, 0, 1, 3, 4}}, // C3
    {{0, 2, 1, 4, 3}}, // C2 on 0
    {{2, 1, 0, 4, 3}}, // C2 on 1
    {{1, 0, 2, 4, 3}} // C2 on 2
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    2
  > tetrahedra {{
      {{0, 1, 3, 2}},
      {{0, 1, 2, 4}}
  }};
};

struct PentagonalPlanar {
  /* 
   * All in plane:
   *
   *      0
   *  1.  |  .4
   *    °(_)°
   *    /   \
   *   2     3
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::PentagonalPlanar;
  static constexpr unsigned size = 5;
  static constexpr char stringName[] = "pentagonal planar";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    unsigned absDiff = std::min(a - b, b - a);
    return std::min(
      absDiff,
      std::min(absDiff - 5, 5 - absDiff)
    ) * ConstexprMagic::Math::toRadians<double>(72);
  }
  static constexpr std::array<ConstexprMagic::Vector, 5> coordinates {{
    {1, 0, 0},
    {0.309017, 0.951057, 0},
    {-0.809017, 0.587785, 0},
    {-0.809017, -0.587785, 0},
    {0.309017, -0.951057, 0}
  }};
  static constexpr std::array<
    std::array<unsigned, 5>,
    2
  > rotations {{
    {{4, 0, 1, 2, 3}}, // C5
    {{0, 4, 3, 2, 1}} // C2
  }};
  static constexpr std::array<
    std::array<unsigned, 4>,
    0
  > tetrahedra {{}};
};

struct Octahedral {
  /* The central atom is ( ), 4 and 5 are axial, the rest equatorial.
   *
   *     4
   *  3  |  2
   *   : | :
   *    (_)        
   *   / | \
   *  0  |  1
   *     5
   *
   * Where /, \ denote bonds in front of the view plane, : denotes bonds
   * behind the view plane.
   */
  static constexpr Symmetry::Name name = Symmetry::Name::Octahedral;
  static constexpr unsigned size = 6;
  static constexpr char stringName[] = "octahedral";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }
    
    if(
      (
        std::max(a, b) < 4 // if the largest is < 4, then equatorial 
        && (a + b) % 2 == 0 // this gives trans eq ligands
      ) || std::min(a, b) == 4 // this indicates 4,5 (axial trans)
    ) {
      return M_PI;
    } 

    return M_PI / 2;
  }
  static constexpr std::array<ConstexprMagic::Vector, 6> coordinates {{
    {1, 0, 0},
    {0, 1, 0},
    {-1, -0, -0},
    {-0, -1, -0},
    {0, 0, 1},
    {-0, -0, -1}
  }};
  static constexpr std::array<
    std::array<unsigned, 6>,
    3
  > rotations {{
    {{3, 0, 1, 2, 4, 5}}, // vertical C4
    {{0, 5, 2, 4, 1, 3}}, // horizontal C4
    {{4, 1, 5, 3, 2, 0}} // horizontal C4'
  }};

#ifdef USE_ALTERNATE_TETRAHEDRA
  static constexpr std::array<
    std::array<unsigned, 4>,
    4
  > tetrahedra {{
    {{3, 0, 4, 5}},
    {{0, 1, 4, 5}},
    {{1, 2, 4, 5}},
    {{2, 3, 4, 5}}
  }};
#else // Regular
  static constexpr std::array<
    std::array<unsigned, 4>,
    8
  > tetrahedra {{
    {{3, 0, 4, ORIGIN_PLACEHOLDER}},
    {{0, 1, 4, ORIGIN_PLACEHOLDER}},
    {{1, 2, 4, ORIGIN_PLACEHOLDER}},
    {{2, 3, 4, ORIGIN_PLACEHOLDER}},
    {{3, 0, ORIGIN_PLACEHOLDER, 5}},
    {{0, 1, ORIGIN_PLACEHOLDER, 5}},
    {{1, 2, ORIGIN_PLACEHOLDER, 5}},
    {{2, 3, ORIGIN_PLACEHOLDER, 5}}
  }};
#endif
};

struct TrigonalPrismatic {
  /* 
   *  3  4  5
   *   : | :
   *    (_)        
   *   : | :
   *  0  1  2
   *
   * Where /, \ denote bonds in front of the view plane, : denotes bonds
   * behind the view plane.
   *
   * Angles 
   *  0-1, 0-2 -> 86°
   *  0-3 -> 76°
   *  0-4, 0-5 -> 134°
   * 
   * From [W(CH3)6], Haaland, Hammel, Rypdal, Volden, J. Am. Chem. Soc. 1990 
   */
  static constexpr Symmetry::Name name = Symmetry::Name::TrigonalPrismatic;
  static constexpr unsigned size = 6;
  static constexpr char stringName[] = "trigonal prismatic";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    } 
    
    // Between plane symmetric
    if(std::min(a - b, b - a) == 3) {
      return ConstexprMagic::Math::toRadians<double>(76);
    } 

    // In plane triangle
    if(
      (a < 3 && b < 3)
      || (a >= 3 && b >= 3)
    ) {
      return ConstexprMagic::Math::toRadians<double>(86);
    } 

    // Between plane asymmetric
    return ConstexprMagic::Math::toRadians<double>(134);
  }
  static constexpr std::array<ConstexprMagic::Vector, 6> coordinates {{
    {0.788011, 0, -0.615661},
    {-0.394005, 0.682437, -0.615661},
    {-0.394005, -0.682437, -0.615661},
    {0.788011, 0, 0.615661},
    {-0.394005, 0.682437, 0.615661},
    {-0.394005, -0.682437, 0.615661}
  }};
  static constexpr std::array<
    std::array<unsigned, 6>,
    2
  > rotations {{
    {{2, 0, 1, 5, 3, 4}}, // C3 axial
    {{5, 4, 3, 2, 1, 0}} // C2 betw. 1, 4
  }};

  static constexpr std::array<
    std::array<unsigned, 4>,
    2
  > tetrahedra {{
    // TODO dubious if this captures all relevant information, too limited
    {{ORIGIN_PLACEHOLDER, 0, 1, 2}},
    {{3, ORIGIN_PLACEHOLDER, 4, 5}}
  }};
};

struct PentagonalPyramidal {
  /* 
   *      0
   *  1.  |  .4
   *    °(5)°
   *    :   :
   *   2     3
   *
   * 0-4 are in plane,
   * 5 is above plane,
   * ( ) signifies the central atom beneath it
   */
  static constexpr Symmetry::Name name = Symmetry::Name::PentagonalPyramidal;
  static constexpr unsigned size = 6;
  static constexpr char stringName[] = "pentagonal pyramidal";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    if(a == 5 || b == 5) {
      return M_PI / 2;
    }  
    
    // remainder are identical to PentagonalPlanar
    unsigned absDiff = std::min(a - b, b - a);
    return std::min(
      absDiff,
      std::min(absDiff - 5, 5 - absDiff)
    ) * ConstexprMagic::Math::toRadians<double>(72);
  }
  static constexpr std::array<ConstexprMagic::Vector, 6> coordinates {{
    {1, 0, 0},
    {0.309017, 0.951057, 0},
    {-0.809017, 0.587785, 0},
    {-0.809017, -0.587785, 0},
    {0.309017, -0.951057, 0},
    {0, 0, 1}
  }};
  static constexpr std::array<
    std::array<unsigned, 6>,
    1
  > rotations {{
    {{4, 0, 1, 2, 3, 5}} // C5 axial
  }};

#ifdef USE_ALTERNATE_TETRAHEDRA
  static constexpr std::array<
    std::array<unsigned, 4>,
    3
  > tetrahedra {{
    {{0, 1, 5, 2}},
    {{2, 3, 5, 4}},
    {{4, 5, ORIGIN_PLACEHOLDER, 0}}
  }};
#else // Regular
  static constexpr std::array<
    std::array<unsigned, 4>,
    5
  > tetrahedra {{
    {{0, ORIGIN_PLACEHOLDER, 1, 5}},
    {{1, ORIGIN_PLACEHOLDER, 2, 5}},
    {{2, ORIGIN_PLACEHOLDER, 3, 5}},
    {{3, ORIGIN_PLACEHOLDER, 4, 5}},
    {{4, ORIGIN_PLACEHOLDER, 0, 5}}
  }};
#endif
};

struct PentagonalBiPyramidal {
    /* 
     * 3, 5, (_) and 6 in plane, 1 and 2 in front, 0 and 4 in back
     *
     *      5
     *  0_  | .4
     *   _:(_) – 3
     *  1   |°·2
     *      6
     *
     * 0-4 are equatorial, 
     * 5,6 are axial
     */
  static constexpr Symmetry::Name name = Symmetry::Name::PentagonalBiPyramidal;
  static constexpr unsigned size = 7;
  static constexpr char stringName[] = "pentagonal bipyramidal";
  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
    if(a == b) {
      return 0;
    }

    if(a + b == 11) {
      return M_PI; // trans 5,6
    }

    if(ConstexprMagic::Math::XOR(a > 4, b > 4)) {
      return M_PI / 2; // any angle to axial index
    }

    // remainder are equatorial angles, like PentagonalPlanar
    unsigned absDiff = std::min(a - b, b - a);
    return std::min(
      absDiff,
      std::min(absDiff - 5, 5 - absDiff)
    ) * ConstexprMagic::Math::toRadians<double>(72);
  }
  static constexpr std::array<ConstexprMagic::Vector, 7> coordinates {{
    {1, 0, 0},
    {0.309017, 0.951057, 0},
    {-0.809017, 0.587785, 0},
    {-0.809017, -0.587785, 0},
    {0.309017, -0.951057, 0},
    {0, 0, 1},
    {-0, -0, -1}
  }};
  static constexpr std::array<
    std::array<unsigned, 7>,
    2
  > rotations {{
    {{4, 0, 1, 2, 3, 5, 6}}, // C5 axial
    {{1, 0, 4, 3, 2, 6, 5}} // C2 equatorial on 3
  }};

#ifdef USE_ALTERNATE_TETRAHEDRA
  static constexpr std::array<
    std::array<unsigned, 4>,
    5
  > tetrahedra {{
      {{0, 1, 5, 6}},
      {{1, 2, 5, 6}},
      {{2, 3, 5, 6}},
      {{3, 4, 5, 6}},
      {{4, 0, 5, 6}}
  }};
#else // Regular
  static constexpr std::array<
    std::array<unsigned, 4>,
    10
  > tetrahedra {{
    {{0, 1, 5, ORIGIN_PLACEHOLDER}},
    {{1, 2, 5, ORIGIN_PLACEHOLDER}},
    {{2, 3, 5, ORIGIN_PLACEHOLDER}},
    {{3, 4, 5, ORIGIN_PLACEHOLDER}},
    {{4, 0, 5, ORIGIN_PLACEHOLDER}},
    {{0, 1, ORIGIN_PLACEHOLDER, 6}},
    {{1, 2, ORIGIN_PLACEHOLDER, 6}},
    {{2, 3, ORIGIN_PLACEHOLDER, 6}},
    {{3, 4, ORIGIN_PLACEHOLDER, 6}},
    {{4, 0, ORIGIN_PLACEHOLDER, 6}}
  }};
#endif
};

struct SquareAntiPrismatic {
  /*  Two representations, one oblique, the other more helpful.
   *  The first is a side-on view. 0 is mostly hidden by 1, 3 mostly hidden 
   *  by 2. 4 and 6 are in the viewing plane while 5 juts out above plane and
   *  7 dips behind plane. 
   *
   *  4   7 5 6
   *    : ·/ :
   *     (__)
   *    ·/  \·
   *   01    23 
   *
   * Below is a top-down view. Strong lines indicate above-plane bonds, dots
   * indicate below-plane bonds.
   *
   *   0   7   3
   *     · | ·
   *   4 –( )– 6
   *     · | ·
   *   1   5   2
   *
   * Two sets of reference coordinates are stored here:
   * - [ReF8]2-, (Koz'min, P.A., Zhurnal Strukturnoi Khimii 1964), retrieved
   *   from the ICSD, Re translated to origin and positions normalized (origin
   *   to symmetry position is length 1)
   * - [W(CN)8]2-, (Alcock, M. W.; Samotus, A.; Szklarzewicz, J. J. Chem. Soc.,
   *   Dalton Trans. 1993, 885. DOI: 10.1039/dt9930000885, CSD: PECMOZ), took
   *   idealized geometry from http://symmetry.otterbein.edu/gallery/ instead
   *   of distorted crystallographic coordinates, normalized lengths
   *
   * Angles (from [ReF8]2-):
   *
   *   in-plane cis (4, 5) -> 55°
   *   in-plane trans (4, 6) -> 148°
   *   short between planes (0, 4) -> 51°
   *   long between planes (0, 5) -> 175°
   *
   */
  static constexpr Symmetry::Name name = Symmetry::Name::SquareAntiPrismatic;
  static constexpr unsigned size = 8;
  static constexpr char stringName[] = "square antiprismatic";
  static constexpr std::array<ConstexprMagic::Vector, 8> coordinates {{
    // [ReF8]2-, distorted crystal structure
    /*{-0.00928803, 0.611568, 0.791137},
    {0.795627, 0.605641, -0.0132684},
    {0.795627, -0.605641, -0.0132684},
    {-0.00928803, -0.611568, 0.791137},
    {-0.396172, 0.852169, -0.341841},
    {0.293758, 0, -0.95588},
    {-0.396172, -0.852169, -0.341841},
    {-0.983087, 0, 0.183141}*/
    // [W(CN)8]2-, idealized to square antiprism
    {-0.23838567,  0.50141283,  0.83171957},
    {-0.7568846,   0.61167543, -0.2301714 },
    { 0.3080136,   0.58106771, -0.75331795},
    { 0.82651172,  0.47080587,  0.30857773},
    {-0.79018301, -0.51909014,  0.32581627},
    {-0.39653401, -0.46341671, -0.79246813},
    { 0.72055552, -0.56338997, -0.40421711},
    { 0.32690564, -0.61906403,  0.71406753},
  }};

#ifdef USE_CONSTEXPR_SQUARE_ANTIPRISMATIC_LOOKUP_TABLE
/*!
 * An upper triangular matrix containing angles between particules i,j in
 * degrees using the square antiprismatic reference coordinates
 */
  static constexpr auto angleLookupTable = ConstexprMagic::makeUpperTriangularMatrix(
    detail::makeArray<size>(coordinates)
  );
#endif

  static constexpr double angleFunction(const unsigned& a, const unsigned& b) {
#ifdef USE_CONSTEXPR_SQUARE_ANTIPRISMATIC_LOOKUP_TABLE
    if(a == b) {
      return 0;
    }

    return angleLookupTable.at(
      std::min(a, b),
      std::max(a, b)
    );
#else
    if(a == b) {
      return 0;
    } 
    
    if(
      (a < 4 && b < 4)
      || (a >= 4 && b >= 4)
    ) { // in plane
      if((a + b) % 2 == 1) { // cis
        return ConstexprMagic::Math::toRadians<double>(72.9875); 
      } 
      
      // otherwise trans
      return ConstexprMagic::Math::toRadians<double>(114.475);
    }
    
    // remaining cases are between planes
    unsigned minDiff = std::min(a - b, b - a);
    if(minDiff == 3 || minDiff == 4 || minDiff == 7) { // short
      return ConstexprMagic::Math::toRadians<double>(78.05);
    } 

    // last case is long between planes
    return ConstexprMagic::Math::toRadians<double>(142.275);
#endif
  }
  static constexpr std::array<
    std::array<unsigned, 8>,
    2
  > rotations {{
    {{3, 0, 1, 2, 7, 4, 5, 6}}, // C4 axial
    /* 180° on equatorial axis in plane with 4, 6 
     *
     *   1   5   2
     *     \ · /
     * – 4 ·( )· 6 –––––– equatorial axis
     *     / · \
     *   0   7   3
     *
     * and 45° anticlockwise on axial axis
     *           
     *   5   2   6
     *     · | ·
     *   1 –( )– 3
     *     · | ·
     *   4   0   7
     *
     */
    {{5, 4, 7, 6, 1, 0, 3, 2}}, 
  }};

#ifdef USE_ALTERNATE_TETRAHEDRA
  static constexpr std::array<
    std::array<unsigned, 4>,
    4
  > tetrahedra {{
      {{0, 1, 4, 6}},
      {{1, 2, 5, 7}},
      {{2, 3, 6, 4}},
      {{3, 0, 7, 5}},
  }};
#else // Regular
  static constexpr std::array<
    std::array<unsigned, 4>,
    8
  > tetrahedra {{
    {{7, 0, 4, ORIGIN_PLACEHOLDER}},
    {{0, 4, ORIGIN_PLACEHOLDER, 1}},
    {{4, 1, 5, ORIGIN_PLACEHOLDER}},
    {{1, 5, ORIGIN_PLACEHOLDER, 2}},
    {{5, 2, 6, ORIGIN_PLACEHOLDER}},
    {{2, 6, ORIGIN_PLACEHOLDER, 3}},
    {{6, 3, 7, ORIGIN_PLACEHOLDER}},
    {{3, 7, ORIGIN_PLACEHOLDER, 0}}
  }};
#endif
};


//! Type collecting all types of the Symmetry classes.
using allSymmetryDataTypes = std::tuple<
  Linear, // 2
  Bent,
  TrigonalPlanar, // 3
  TrigonalPyramidal,
  TShaped,
  Tetrahedral, // 4
  SquarePlanar,
  Seesaw,
  SquarePyramidal, // 5
  TrigonalBiPyramidal,
  PentagonalPlanar,
  Octahedral, // 6
  TrigonalPrismatic,
  PentagonalPyramidal,
  PentagonalBiPyramidal, // 7
  SquareAntiPrismatic // 8
>;

using limitedSymmetryDataTypes = std::tuple<
  Linear, // 2
  Bent,
  TrigonalPlanar, // 3
  TrigonalPyramidal,
  TShaped,
  Tetrahedral, // 4
  SquarePlanar,
  Seesaw,
  SquarePyramidal, // 5
  TrigonalBiPyramidal,
  PentagonalPlanar
>;

// Typedef to avoid reusing C-Style function ptr type
using AngleFunctionPtr = double(*)(const unsigned&, const unsigned&);

/*!
 * Constructs an array of function pointers to all static angle functions
 * for runtime lookup
 */
template<typename ...SymmetryClasses>
struct angleFunctionFunctor {
  static constexpr std::array<
    data::AngleFunctionPtr,
    sizeof...(SymmetryClasses)
  > value() {
    return {{
      &SymmetryClasses::angleFunction...
    }};
  }
};

/*! Conversion function to make the dynamic rotations list type from the
 * constexpr data types given in a specifc symmetry class type
 */
template<size_t symmetrySize, size_t nRotations>
RotationsList makeRotations(
  const std::array<
    std::array<unsigned, symmetrySize>,
    nRotations
  >& constexprRotations
) {
  RotationsList rotations;

  for(const auto& rotation : constexprRotations) {
    rotations.emplace_back(
      rotation.begin(),
      rotation.end()
    );
  }

  return rotations;
}

/*! Conversion function to make the dynamic tetrahedron list type from the
 * constexpr data types given in a specifc symmetry class type
 */
template<size_t nTetrahedra>
TetrahedronList makeTetrahedra(
  const std::array<
    std::array<unsigned, 4>,
    nTetrahedra
  >& constexprTetrahedra
) {
  TetrahedronList tetrahedra;

  for(const auto& tetrahedron : constexprTetrahedra) {
    tetrahedra.push_back(
      TemplateMagic::map(
        tetrahedron,
        [](const unsigned& index) -> boost::optional<unsigned> {
          if(index == ORIGIN_PLACEHOLDER) {
            return boost::none;
          }

          return index;
        }
      )
    );
  }

  return tetrahedra;
}

//! Conversion helper to Eigen type from constexpr vector type
Eigen::Vector3d toEigen(const ConstexprMagic::Vector& cVector);

/*! Conversion function to make the dynamic coordinates list type from the
 * constexpr data types given in a specifc symmetry class type
 */
template<size_t symmetrySize>
CoordinateList makeCoordinates(
  const std::array<ConstexprMagic::Vector, symmetrySize>& constexprCoordinates
) {
  return TemplateMagic::mapToVector(
    constexprCoordinates,
    toEigen
  );
}

/*! This constructs the SymmetryInformation instance from a specific symmetry
 * class type.
 */
template<typename SymmetryClass>
SymmetryInformation makeSymmetryInformation() {
  return {
    SymmetryClass::stringName,
    SymmetryClass::size,
    makeRotations(SymmetryClass::rotations),
    makeTetrahedra(SymmetryClass::tetrahedra),
    makeCoordinates(SymmetryClass::coordinates)
  };
}

/*! This creates a map initialization pair for a specific symmetry class type.
 * The key is the name, the mapped_type a SymmetryInformation instance
 */
template<typename SymmetryClass>
std::pair<Name, SymmetryInformation> makeMapInitPair() {
  return {
    SymmetryClass::name,
    makeSymmetryInformation<SymmetryClass>()
  };
}

/*! Creates the mapping between a symmetry class's name and it's dynamic
 * information in order to have runtime lookup based on symmetry names.
 */
template<typename ...SymmetryClasses>
struct symmetryInformationFunctor {
  static const std::map<Name, SymmetryInformation> value() {
    return {{
      makeMapInitPair<SymmetryClasses>()...
    }};
  }
};

constexpr auto angleFunctions = ConstexprMagic::TupleType::unpackToFunction<
  allSymmetryDataTypes,
  angleFunctionFunctor
>();

} // namespace data

/* Core symmetry data, this has dynamic types and is hence initialized in the
 * .cpp file from the tuple containing all symmetry data types and the 
 * symmetryInformationFunctor
 */
const std::map<Name, SymmetryInformation>& symmetryData();

/* Interface */
//! Fetch the string name of a symmetry
inline const std::string& name(const Name& name) {
  return symmetryData().at(name).stringName;
}

//! Fetch a space-free name for file naming
inline std::string spaceFreeName(const Name& name) {
  std::string toModify = symmetryData().at(name).stringName;

  std::replace(
    toModify.begin(),
    toModify.end(),
    ' ',
    '-'
  );

  return toModify;
}

//! Fetch the number of symmetry positions of a symmetry
inline const unsigned& size(const Name& name) {
  return symmetryData().at(name).size;
}

//! Fetches a symmetry's list of rotations
inline const RotationsList& rotations(const Name& name) {
  return symmetryData().at(name).rotations;
}

//! Gets a symmetry's angle function
inline data::AngleFunctionPtr angleFunction(const Name& name) {
  unsigned symmetryIndex = static_cast<unsigned>(name);
  return data::angleFunctions.at(symmetryIndex);
}

//! Returns the index of a symmetry name within allNames
inline unsigned nameIndex(const Name& name) {
  return std::find(
    allNames.begin(),
    allNames.end(),
    name
  ) - allNames.begin();
}

//! Fetches the list of tetrahedra defined in a symmetry
inline const TetrahedronList& tetrahedra(const Name& name) {
  return symmetryData().at(name).tetrahedra;
}

} // namespace Symmetry

#endif
