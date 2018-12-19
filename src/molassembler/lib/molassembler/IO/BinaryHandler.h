/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief Binary Input/output
 */

#ifndef INCLUDE_MOLASSEMBLER_IO_BINARY_H
#define INCLUDE_MOLASSEMBLER_IO_BINARY_H

#include <fstream>
#include <string>
#include <vector>
#include <bitset>

namespace Scine {

namespace molassembler {

// Forward-declarations
class Molecule;

namespace IO {

//! Binary file IO
struct BinaryHandler {
  using BinaryType = std::vector<std::uint8_t>;

  template<typename T>
  static std::enable_if_t<
    std::is_unsigned<T>::value,
    void
  > write(
    std::ofstream& file,
    const T value
  ) {
    std::bitset<8 * sizeof(T)> bits {value};
    file << bits;
  }

  template<typename T>
  static std::enable_if_t<
    std::is_unsigned<T>::value,
    T
  > read(std::ifstream& file) {
    std::bitset<8 * sizeof(T)> bits;
    file >> bits;
    return static_cast<T>(
      bits.to_ulong()
    );
  }

  static bool canRead(const std::string& filename);

  static void write(
    const std::string& filename,
    const BinaryType& binary
  );

  static BinaryType read(const std::string& filename);
};

} // namespace IO

} // namespace molassembler

} // namespace Scine

#endif
