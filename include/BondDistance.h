#ifndef INCLUDE_BOND_DISTANCE_H
#define INCLUDE_BOND_DISTANCE_H

#include <cmath>
#include <array>
#include "Delib/ElementTypes.h" // Delib

#include "AtomInfo.h"
#include "common_typedefs.h"

namespace MoleculeManip {

namespace Bond {

static constexpr std::array<double, 8> bondOrderMap {{
  1, 2, 3, 4, 5, 6, 1.5, 0.5
}};

constexpr double bondOrderCorrectionLambda = 0.1332;

double calculateBondDistance(
  const Delib::ElementType& a,
  const Delib::ElementType& b,
  const BondType& bondType
);

} // namespace Bond

} // namespace MoleculeManip

#endif

