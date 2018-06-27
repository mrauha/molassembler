#include "AtomEnvironmentHash.h"

#include "chemical_symmetries/Properties.h"

namespace molassembler {

namespace hashes {

AtomEnvironmentHashType atomEnvironment(
  const temple::Bitmask<AtomEnvironmentComponents>& bitmask,
  const Delib::ElementType elementType,
  const std::vector<BondType>& sortedBonds,
  boost::optional<Symmetry::Name> symmetryNameOptional,
  boost::optional<unsigned> assignedOptional
) {
  static_assert(
    // Sum of information in bits we want to pack in the hash
    (
      // Element type (fixed as this cannot possibly increase)
      7
      // Bond type, exactly as many as the largest possible symmetry
      + 4 * Symmetry::constexprProperties::maxSymmetrySize
      // The symmetry name
      + temple::Math::ceil(
        temple::Math::log(Symmetry::nSymmetries + 1.0, 2.0)
      )
      // Roughly 6.5k possible assignment values (maximally asymmetric square antiprismatic)
      + 13
    ) <= 64,
    "Element type, bond and symmetry information no longer fit into a 64-bit unsigned integer"
  );


  /* First 8 bits of the 64 bit unsigned number are from the element type
   *
   * Biggest is Cn, which has a value of 112 -> Fits in 7 bits (2^7 = 128)
   */
  AtomEnvironmentHashType value;
  if(bitmask & AtomEnvironmentComponents::ElementTypes) {
    value = static_cast<AtomEnvironmentHashType>(elementType);
  } else {
    value = 0;
  }

  /* Bonds have 8 possible values currently, plus None is 9
   * -> fits into 4 bits (2^4 = 16)
   *
   * (You could make an argument for removing Sextuple from the list of bond
   * types and fitting this precisely into 3 bits only)
   *
   * So, left shift by 7 bits (so there are 7 zeros on the right in the bit
   * representation) plus the current bond number multiplied by 4 to place a
   * maximum of 8 bonds (maximum symmetry size currently)
   *
   * This occupies 4 * 8 = 32 bits.
   */
  if(bitmask & AtomEnvironmentComponents::BondOrders) {
    unsigned bondNumber = 0;
    for(const auto& bond : sortedBonds) {
      value += (
        // No bond is represented by 0, while the remaining bond types are shifted
        static_cast<AtomEnvironmentHashType>(bond) + 1
      ) << (7 + 4 * bondNumber);

      ++bondNumber;
    }
  }

  if((bitmask & AtomEnvironmentComponents::Symmetries) && symmetryNameOptional) {
    /* We add symmetry information on non-terminal atoms. There are currently
     * 16 symmetries, plus None is 17, which fits into 5 bits (2^5 = 32)
     */
    value += (static_cast<AtomEnvironmentHashType>(symmetryNameOptional.value()) + 1) << 39;

    if(bitmask & AtomEnvironmentComponents::Stereopermutations) {
      /* The remaining space 64 - (7 + 32 + 5) = 20 bits is used for the current
       * permutation. In that space, we can store up to 2^20 - 2 > 1e5
       * permutations (one (0) for no stereocenter, one (1) for unassigned),
       * which ought to be plenty of bit space. Maximally asymmetric square
       * antiprismatic has around 6k permutations, which fits into 13 bits.
       */
      AtomEnvironmentHashType permutationValue;
      if(assignedOptional) {
        permutationValue = assignedOptional.value() + 2;
      } else {
        permutationValue = 1;
      }
      value += static_cast<AtomEnvironmentHashType>(permutationValue) << 44;
    }
  }

  return value;
}

} // namespace hashes

} // namespace molassembler