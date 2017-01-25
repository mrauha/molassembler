#include "Symmetries.h"

#include <cassert>

namespace Symmetry {

const std::vector<Name> allNames {
  Name::Linear, // 2
  Name::Bent,
  Name::TrigonalPlanar, // 3
  Name::TrigonalPyramidal,
  Name::TShaped,
  Name::Tetrahedral, // 4
  Name::SquarePlanar,
  Name::Seesaw,
  Name::SquarePyramidal, // 5
  Name::TrigonalBiPyramidal,
  Name::PentagonalPlanar,
  Name::Octahedral, // 6
  Name::TrigonalPrismatic,
  Name::PentagonalPyramidal,
  Name::PentagonalBiPyramidal, // 7
  Name::SquareAntiPrismatic // 8
};

const std::map<Name, TupleType> symmetryData {
  {Name::Linear, TupleType(
    /* 0 – (_) – 1 */
    "linear",
    2,
    {
      {1, 0}
    },
    [](
      const unsigned& a __attribute__ ((unused)),
      const unsigned& b __attribute__ ((unused))
    ) -> double {
      return 180;
    }
  )},
  {Name::Bent, TupleType(
    /*
     *    (_)  
     *   /   \
     *  0     1
     *
     */
    "bent",
    2,
    {
      {1, 0}
    },
    [](
      const unsigned& a __attribute__ ((unused)),
      const unsigned& b __attribute__ ((unused))
    ) -> double { 
      /* subject to a lot of variation, between 90 and 109 degrees pursuant to 
       * english wikipedia, using experimental data here to improve instances
       * of this geometry on e.g. O center would a big improvement to DG runs
       */
      return 107; 
    }
  )},
  {Name::TrigonalPlanar, TupleType(
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
    "trigonal planar",
    3,
    {
      {1, 2, 0}, // C3
      {0, 2, 1} // C2
    },
    [](
      const unsigned& a __attribute__ ((unused)),
      const unsigned& b __attribute__ ((unused))
    ) -> double {
      return 120;
    }
  )}, 
  {Name::TrigonalPyramidal, TupleType(
    /*
     *    
     *   (_)   
     *  : | :
     * 0  1  2
     *
     */
    "trigonal pyramidal",
    3,
    {
      {2, 0, 1} // C3
    },
    [](
      const unsigned& a __attribute__ ((unused)),
      const unsigned& b __attribute__ ((unused))
    ) -> double {
      return 107;
    }
  )}, 
  {Name::TShaped, TupleType(
    /*
     * 0 – (_) – 2
     *      |
     *      1
     *
     */
    "T-shaped",
    3,
    {
      {2, 1, 0} // C2
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      if(a + b % 2 == 1) {
        return 90;
      } else {
        return 180;
      }
    }
  )}, 
  {Name::Tetrahedral, TupleType(
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
    "tetrahedral",
    4,
    {
      {0, 3, 1, 2}, // C4, 1
      {2, 1, 3, 0}, // C4, 2
      {3, 0, 2, 1}, // C4, 3
      {1, 2, 0, 3}  // C4, 4
    },
    [](
      const unsigned& a __attribute__ ((unused)),
      const unsigned& b __attribute__ ((unused))
    ) -> double {
      return 109.5;
    }
  )}, 
  {Name::SquarePlanar, TupleType(
    /* 3   2
     *  \_/
     *  (_) <- central atom
     *  / \
     * 0   1
     *
     */
    "square planar",
    4,
    {
      {3, 0, 1, 2}, // C4
      {1, 0, 3, 2}, // C2
      {3, 2, 1, 0}  // C2'
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      assert(a < 4 && b < 4 && a != b);
      if((a + b) % 2 == 1) {
        // this expression indicates cis
        return 90;
      } else {
        // are trans
        return 180;
      }
    }
  )}, 
  {Name::Seesaw, TupleType(
    /*
     * 0 – (_) – 3
     *     / :
     *    1   2
     *
     */
    "seesaw",
    4,
    {
      {3, 2, 1, 0} // C2
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      if(a == 0 && b == 3) return 180;
      else if(a == 1 && b == 2) return 120;
      else return 90;
    }
  )}, 
  {Name::SquarePyramidal, TupleType(
    /* 4   3
     *  \_/
     *  (5)   
     *  / \
     * 1   2
     *
     * Viewed from the top of the pyramid. The central atom is ( ), 5 is axial.
     *
     * Alternatively
     *
     *    5
     *    |
     * 4  |  3
     *  : | :    <- behind view plane
     *   (_)
     *  /   \    <- in front of view plane
     * 1     2
     *
     */
    "square pyramidal",
    5,
    {
      {3, 0, 1, 2, 4} // C4
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      assert(a < 5 && b < 5 && a != b && a < b);
      if(b < 4 && (a + b) % 2 == 0) {
        /* as long as b is not the pyramidal ligand, 
         * (a + b) % 2 == 0 is true for trans ligands only
         */
        return 180;
      } else { 
        // all other cases are
        return 90;
      }
    }
  )}, 
  {Name::TrigonalBiPyramidal, TupleType(
    /* Viewed from the top of the pyramid. The central atom is ( ), 4 and 5 
     * are axial.
     *
     *     4
     *     |  3
     *     | :    <- behind view plane
     * 1--(_)
     *     | \    <- in front of view plane
     *     |  2
     *     5
     */
    "trigonal bipyramidal",
    5,
    {
      {2, 0, 1, 3, 4}, // C3
      {0, 2, 1, 4, 3}, // C2 on 1
      {2, 1, 0, 4, 3}, // C2 on 2
      {1, 0, 2, 4, 3} // C2 on 3
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      assert(a < 5 && b < 5 && a != b && a < b);
      if(b < 3) {
        // since a < b, this means either 0,1 0,2 1,2 
        return 120;
      } else if(b == 3) {
        // since a < b, this means {1,2,3}, 3 
        return 90;
      } else if(a < 3) {
        // now, b must be 4, and if a is not 3, then
        return 90;
      } else {
        // only case left: 3,4
        return 180;
      }
    }
  )}, 
  {Name::PentagonalPlanar, TupleType(
    /* 
     *      0
     *  1.  |  .4
     *    °(_)°
     *    :   :
     *   2     3
     *
     */
    "pentagonal planar",
    5,
    {
      {4, 0, 1, 2, 3}, // C5
      {0, 4, 3, 2, 1} // C2
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      unsigned absDiff = std::min(a - b, b - a);
      return std::min(
        absDiff,
        std::min(absDiff - 5, 5 - absDiff)
      ) * 72;
    }
  )}, 
  {Name::Octahedral, TupleType(
    /* The central atom is ( ), 5 and 6 are axial, the rest equatorial.
     *
     *     5
     *  4  |  3
     *   : | :
     *    (_)        
     *   / | \
     *  1  |  2
     *     6
     *
     * Where /, \ denote bonds in front of the view plane, : denotes bonds
     * behind the view plane.
     */
    "octahedral",
    6,
    {
      {3, 0, 1, 2, 4, 5}, // vertical C4
      {0, 5, 2, 4, 1, 3}, // horizontal C4
      {4, 1, 5, 3, 2, 0} // horizontal C4'
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      assert(a < 6 && b < 6 && a != b && a < b);
      if(
        (
          b < 4 // if b < 4, then only equatorial ligands
          && (a + b) % 2 == 0 // this gives trans eq ligands
        ) || a == 4 // this indicates 4,5 (axial trans)
      ) {
        return 180;
      } else {
        return 90;
      }
    }
  )},
  {Name::TrigonalPrismatic, TupleType(
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
    "trigonal prismatic",
    6,
    {
      {2, 0, 1, 5, 3, 4}, // C3 axial
      {5, 4, 3, 2, 1, 0} // C2 betw. 1, 4
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      assert(a != b);
      if(std::min(a - b, b - a) == 3) return 76;
      else {
        if(
          (a < 3 && b < 3)
          || (a >= 3 && b >= 3)
        ) {
          return 86;
        } else {
          return 134;
        }
      }
    }
  )},
  {Name::PentagonalPyramidal, TupleType(
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
    "pentagonal pyramidal",
    6,
    {
      {4, 0, 1, 2, 3, 5} // C5 axial
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      if(a == 5 || b == 5) {
        return 90;
      } else { // identical to PentagonalPlanar
        unsigned absDiff = std::min(a - b, b - a);
        return std::min(
          absDiff,
          std::min(absDiff - 5, 5 - absDiff)
        ) * 72;
        }
    }
  )},
  {Name::PentagonalBiPyramidal, TupleType(
    /* 
     *      5
     *  0_  | .4
     *   _:(_) – 3
     *  1   |°·2
     *      6
     *
     * 0-4 are equatorial, 
     * 5,6 are axial
     */
    "pentagonal bipyramidal",
    7,
    {
      {4, 0, 1, 2, 3, 5, 6}, // C5 axial
      {1, 0, 4, 3, 2, 6, 5} // C2 equatorial on 3
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      if(a + b == 11) return 180; // cis 5,6
      else if(a + b > 5) return 90; // any angle to axial index
      else { // equatorial angles, like PentagonalPlanar
        unsigned absDiff = std::min(a - b, b - a);
        return std::min(
          absDiff,
          std::min(absDiff - 5, 5 - absDiff)
        ) * 72;
      }
    }
  )},
  {Name::SquareAntiPrismatic, TupleType(
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
     * Angles:
     *   in-plane cis (4, 5) -> 55°
     *   in-plane trans (4, 6) -> 148°
     *   short between planes (0, 4) -> 51°
     *   long between planes (0, 5) -> 175°
     *
     * from [ReF8]2-, Koz'min, P.A., Zhurnal Strukturnoi Khimii 1964 
     * HINT: use the ICSD for angle calculations and preview
     *
     */
    "square antiprismatic",
    8,
    {
      {3, 0, 1, 2, 7, 4, 5, 6}, // C4 axial
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
      {5, 4, 7, 6, 1, 0, 3, 2}, 
    },
    [](
      const unsigned& a,
      const unsigned& b
    ) -> double {
      if(
        (a < 4 && b < 4)
        || (a >= 4 && b >= 4)
      ) { // in plane
        if(a + b % 2 == 1) return 55; // cis 
        else return 148;
      } else { // between planes
        unsigned minDiff = std::min(a - b, b - a);
        if(minDiff == 3 || minDiff == 4 || minDiff == 7) { // short
          return 51;
        } else {
          return 175;
        }
      }
    }
  )},
};

const std::string& name(const Name& name) {
  return std::get<0>(
    symmetryData.at(name)
  );
}

unsigned size(const Name& name) {
  return std::get<1>(
    symmetryData.at(name)
  );
}

const RotationsType& rotations(const Name& name) {
  return std::get<2>(
    symmetryData.at(name)
  );
}

const AngleFunctionType& angleFunction(const Name& name) {
  return std::get<3>(
    symmetryData.at(name)
  );
}

} // eo namespace
