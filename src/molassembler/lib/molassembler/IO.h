#ifndef INCLUDE_MOLECULE_IO_H
#define INCLUDE_MOLECULE_IO_H

#include "boost/bimap.hpp"

#include "temple/Random.h"

#include "Interpret.h"

/*! @file
 *
 * Contains main IO definitions of the library. Currently only supports
 * MOLFile V2000 specification.
 */

/* TODO
 * - test V2000
 * - implement V3000
 * - Read coordinates are currently discarded. Either process them for
 *   information or use them to check against the CTAB
 */

namespace molassembler {

//! Input and output
namespace IO {

namespace permutations {

//! Maps any type T provided to itself
struct Identity {
  template<typename T>
  T operator() (const T& t) const {
    return t;
  }
};

//! Randomizes atom indices.
struct Random {
  std::vector<AtomIndexType> permutation;

  inline explicit Random(AtomIndexType N) {
    permutation.resize(N);
    std::iota(permutation.begin(), permutation.end(), 0);

    temple::random.shuffle(permutation);
  }

  inline AtomIndexType operator() (const AtomIndexType& i) const {
    return permutation.at(i);
  }
};

//! Sorts an element's indices by atom elements' Z
struct SortByElement {
  std::vector<AtomIndexType> permutation;

  inline explicit SortByElement(const Molecule& mol) {
    permutation.resize(mol.numAtoms());
    std::iota(permutation.begin(), permutation.end(), 0);
    std::sort(
      permutation.begin(),
      permutation.end(),
      [&mol](const AtomIndexType& i, const AtomIndexType& j) -> bool {
        return mol.getElementType(i) < mol.getElementType(j);
      }
    );
  }

  inline AtomIndexType operator() (const AtomIndexType& i) const {
    return permutation.at(i);
  }
};

//! Provides the inverse permutation to any permutation
struct Inverse {
  std::vector<AtomIndexType> permutation;

  template<typename Permutation>
  inline explicit Inverse(const Permutation& ante, const AtomIndexType& size) {
    permutation.resize(size);
    for(AtomIndexType i = 0; i < size; ++i) {
      permutation.at(ante(i)) = i;
    }
  }

  inline AtomIndexType operator() (const AtomIndexType& i) const {
    return permutation.at(i);
  }
};

} // namespace permutations

//! Abstract base class to all file handlers
struct FileHandler {
  enum class IndexPermutation {
    Identity,
    SortByElement,
    Random
  };

  struct RawData {
    Delib::ElementTypeCollection elements;
    AngstromWrapper angstromWrapper;
    Delib::BondOrderCollection bondOrders;
  };

  // Virtualize destructor
  virtual ~FileHandler() = default;

  // Demand strong const guarantees for quality of implementation
  virtual bool canRead(const std::string& filename) const = 0;
  virtual RawData read(const std::string& filename) const = 0;
  virtual void write(
    const std::string& filename,
    const Molecule& molecule,
    const AngstromWrapper& angstromWrapper,
    const IndexPermutation& permutation = IndexPermutation::Identity
  ) const = 0;
};

//! MOL file IO
class MOLFileHandler final : public FileHandler {
private:
/* Typedefs */
  enum class State {
    HeaderMoleculeName,
    HeaderProgram,
    HeaderComments,
    CountsLine,
    AtomBlock,
    BondBlock,
    End
  };

  enum class MOLFileVersion {
    V2000
    // V3000
  };

  // Version string from file <-> internal enum capturing current version
  static const boost::bimap<std::string, MOLFileVersion> _versionMap;

  // MOLFile bond type value <-> internal bond type enum
  static const boost::bimap<unsigned, BondType> _bondTypeMap;

/* Private static functions */
  static std::string _removeAllSpaces(std::string&& a);
  static std::string _removeAllSpaces(const std::string& a);

/* Private member functions */

  /* NOTE: Important cases, perhaps to be altered
   * TODO If BondTypes of 4-6 are to be stored in a MOLFile, the program will
   * crash, no elegant failure exists.
   */
  void _write(
    const std::string& filename,
    const Molecule& molecule,
    const AngstromWrapper& angstromWrapper,
    const MOLFileVersion& version,
    const IndexPermutation& permutation
  ) const;

public:
/* Public types */
/* Public member functions */
  /* Reading functions */
  bool canRead(const std::string& filename) const final;

  RawData read(const std::string& filename) const final;

  //! Write a single Molecule to a file
  void write(
    const std::string& filename,
    const Molecule& molecule,
    const AngstromWrapper& angstromWrapper,
    const IndexPermutation& permutation = IndexPermutation::Identity
  ) const final;
};

//! XYZ file IO
struct XYZHandler final : public FileHandler {
  bool canRead(const std::string& filename) const final;

  RawData read(const std::string& filename) const final;

  //! Write a single Molecule to a file
  void write(
    const std::string& filename,
    const Molecule& molecule,
    const AngstromWrapper& angstromWrapper,
    const IndexPermutation& permutation = IndexPermutation::Identity
  ) const final;
};

//! Binary file IO
struct BinaryHandler {
  using BinaryType = std::vector<std::uint8_t>;

  static bool canRead(const std::string& filename);

  static void write(
    const std::string& filename,
    const BinaryType& binary
  );

  static BinaryType read(const std::string& filename);
};

namespace detail {

InterpretResult interpret(const FileHandler::RawData& data);

} // namespace detail

/*! Read a single molecule from a file.
 *
 * @throws If interpretation of coordinates and connectivity yields multiple
 *   molecules.
 * @note Interprets file type from extension. mol is a MOLFile, xyz an XYZ file
 *   and masm a CBOR serial representation of Molecule
 */
Molecule read(const std::string& filename);

/*! Read multiple molecules from a file.
 *
 * @note Interprets file type from extension. mol is a MOLFile, xyz an XYZ file
 *   and masm a CBOR serial representation of Molecule
 */
std::vector<Molecule> split(const std::string& filename);

/*! Writer function for MOL and XYZ formats
 *
 * @throws If the file extension does not match either mol or xyz.
 * @note Interprets which file type is to be written from filename extension.
 */
void write(
  const std::string& filename,
  const Molecule& molecule,
  const AngstromWrapper& angstromWrapper,
  const FileHandler::IndexPermutation permutation = FileHandler::IndexPermutation::Identity
);

//! Writer function from a PositionCollection in bohr
void write(
  const std::string& filename,
  const Molecule& molecule,
  const Delib::PositionCollection& positions,
  const FileHandler::IndexPermutation permutation = FileHandler::IndexPermutation::Identity
);

/*! Writer function to make files containing binary Molecule representation.
 *
 * @throws If the file extension does not match .masm
 */
void write(
  const std::string& filename,
  const Molecule& molecule
);

} // namespace IO

} // namespace molassembler

#endif
