/*!@file
 * @copyright This code is licensed under the 3-clause BSD license.
 *   Copyright ETH Zurich, Laboratory of Physical Chemistry, Reiher Group.
 *   See LICENSE.txt for details.
 * @brief Provides serialization / deserialization for Molecule instances
 */

#ifndef INCLUDE_MOLASSEMBLER_SERIALIZATION_H
#define INCLUDE_MOLASSEMBLER_SERIALIZATION_H

#include <vector>
#include <string>
#include <memory>

#include "Molassembler/Export.h"

namespace Scine {
namespace Molassembler {

// Forward-declarations
class Molecule;

/**
 * @brief Class representing a compact JSON serialization of a molecule
 *
 * The JSON representation, although principally human readable, is very
 * compact. Keys are heavily shortened. Each Molecule JSON Object has the
 * following structure:
 *
 * @verbatim
 * - a: List of AtomStereopermutator Objects
 *   - a: Assignment index (key omitted if unassigned)
 *   - c: Central index
 *   - r: Ranking
 *     - s: Sorted subtituents
 *     - l: Ligands
 *     - lr: Ranked ligands
 *     - lnk: Links (key omitted if empty)
 *   - s: Shape name index
 * - b: List of BondStereopermutator Objects
 *   - a: Assignment index (key omitted if unassigned)
 *   - e: Edge on which it is placed
 * - c: Canonicalization state
 * - g: Graph Object
 *   - Z: List of atomic numbers
 *   - E: List of edges, each a List
 *     - 0: Source vertex
 *     - 1: Target vertex
 *     - 2: Bond type index
 * - v: Library version List
 *   - 0: Major
 *   - 1: Minor
 *   - 2: Patch
 * @endverbatim
 *
 * For std::string - Molecule interconversions, you can implicitly call the
 * converting operators:
 * @code{cpp}
 * Molecule mol;
 * std::string jsonSerialization = JsonSerialization(mol);
 * Molecule reverted = JsonSerialization(jsonSerialization);
 * @endcode
 *
 * For BinaryType - Molecule interconversions, only the implicit Molecule
 * conversion operator is available since the binary JSON format must be
 * specified:
 * @code{cpp}
 * Molecule mol;
 * JsonSerialization::BinaryType bson = JsonSerialization(mol).toBinary(JsonSerialization::BinaryFormat::BSON);
 * Molecule reverted = JsonSerialization(bson, JsonSerialization::BinaryFormat::BSON);
 * @endcode
 *
 * @warning Serializations of molecules have substantial notational freedom.
 * Using lexicographical comparison on serializations does not have the same
 * semantics as calling Molecule's relational operators.
 */
class MASM_EXPORT JsonSerialization {
public:
//!@name Public types
//!@{
  //! Type used to represent binary JSON formats
  using BinaryType = std::vector<std::uint8_t>;

  //! Binary formats that JSON can be encoded into and decoded from
  enum class BinaryFormat {
    CBOR,
    BSON,
    MsgPack,
    UBJSON
  };
//!@}

//!@name Static members
//!@{
  static std::string base64Encode(const BinaryType& binary);
  static BinaryType base64Decode(const std::string& base64String);
//!@}

//!@name Special member functions
//!@{
  JsonSerialization(JsonSerialization&& other) noexcept;
  JsonSerialization& operator = (JsonSerialization&& other) noexcept;
  JsonSerialization(const JsonSerialization& other);
  JsonSerialization& operator = (const JsonSerialization& other);
  ~JsonSerialization();
//!@}

//!@name Constructors
//!@{
  JsonSerialization() = delete;
  //! Construct a serialization from a JSON string
  explicit JsonSerialization(const std::string& jsonString);
  //! Construct a serialization from a Molecule
  explicit JsonSerialization(const Molecule& molecule);
  //! Construct a serialization from a binary JSON format
  JsonSerialization(const BinaryType& binary, BinaryFormat format);
//!@}

//!@name Conversions
//!@{
  //! Dump the unprettified JSON notation as a string
  operator std::string() const;
  //! Deserialize the JSON serialization into a Molecule
  operator Molecule() const;
  //! Serialize the JSON serialization into a binary JSON format
  BinaryType toBinary(BinaryFormat format) const;
//!@}

//!@name Modification
//!@{
  /** @brief Eliminate all notational freedom of the JSON serialization
   *
   * The Molecule's JSON representation notational freedoms are removed:
   * - Edges have ordered indices
   * - The graph edge list is sorted
   * - The lists of atom and bond stereopermutators are sorted by their placement
   *   atoms and bonds, respectively
   * - Ranking information notational freedom is removed
   *
   * @complexity{@math{\Theta(V + E + A + B)}}
   *
   * @throws std::logic_error If the underlying molecule is not fully canonical.
   * It makes zero sense to standardize the JSON representation if the molecule
   * itself is not canonicalized since then JSON representation comparison will
   * still not yield the same behavior as Molecule comparison.
   *
   * @returns *this for method chaining
   */
  JsonSerialization& standardize();
//!@}

private:
  struct Impl;
  std::unique_ptr<Impl> pImpl_;
};

} // namespace Molassembler
} // namespace Scine

#endif
