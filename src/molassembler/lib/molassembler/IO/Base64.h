/*!@file
 * @copyright ETH Zurich, Laboratory for Physical Chemistry, Reiher Group.
 *   See LICENSE.txt
 * @brief Base 64 encoding and decoding between strings and vectors of uint_8
 */

#ifndef INCLUDE_BASE_64_ENCODING_H
#define INCLUDE_BASE_64_ENCODING_H

#include <string>
#include <vector>
#include <stdexcept>

namespace Scine {

namespace base64 {

/* Adapted from public domain licensed code at
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Miscellaneous/Base64
 */

// Lookup table
// If you want to use an alternate alphabet, change the characters here
constexpr char encodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr char padCharacter = '=';

/** @brief Encode binary data as a string
 *
 * @complexity{@math{\Theta(N)}}
 *
 * @param inputBuffer binary data
 */
std::string encode(const std::vector<std::uint8_t>& inputBuffer) {
	std::string encodedString;
	encodedString.reserve(
    (
      (inputBuffer.size() / 3)
      + (inputBuffer.size() % 3 > 0)
    ) * 4
  );
	long temp;
	std::vector<std::uint8_t>::const_iterator cursor = inputBuffer.begin();

	for(size_t idx = 0; idx < inputBuffer.size()/3; idx++) {
		temp  = (*cursor++) << 16; //Convert to big endian
		temp += (*cursor++) << 8;
		temp += (*cursor++);
		encodedString.append(1, encodeLookup[(temp & 0x00FC0000) >> 18]);
		encodedString.append(1, encodeLookup[(temp & 0x0003F000) >> 12]);
		encodedString.append(1, encodeLookup[(temp & 0x00000FC0) >> 6 ]);
		encodedString.append(1, encodeLookup[(temp & 0x0000003F)      ]);
	}

	switch(inputBuffer.size() % 3) {
	case 1:
		temp  = (*cursor++) << 16; //Convert to big endian
		encodedString.append(1, encodeLookup[(temp & 0x00FC0000) >> 18]);
		encodedString.append(1, encodeLookup[(temp & 0x0003F000) >> 12]);
		encodedString.append(2, padCharacter);
		break;
	case 2:
		temp  = (*cursor++) << 16; //Convert to big endian
		temp += (*cursor++) << 8;
		encodedString.append(1, encodeLookup[(temp & 0x00FC0000) >> 18]);
		encodedString.append(1, encodeLookup[(temp & 0x0003F000) >> 12]);
		encodedString.append(1, encodeLookup[(temp & 0x00000FC0) >> 6 ]);
		encodedString.append(1, padCharacter);
		break;
	}

	return encodedString;
}

/** @brief Decode base 64 string data to binary
 *
 * @complexity{@math{\Theta(N)}}
 *
 * @param input base 64 string
 */
std::vector<std::uint8_t> decode(const std::string& input) {
  const std::size_t length = input.length();

	if(length % 4) {
		throw std::runtime_error("Invalid base64 encoding!");
  }

  std::size_t padding = 0;
  if(length > 0) {
    padding = (
      static_cast<std::size_t>(input[length - 1] == padCharacter)
      + static_cast<std::size_t>(input[length - 2] == padCharacter)
    );
  }

	std::vector<std::uint8_t> decodedBytes;
	decodedBytes.reserve(((length/4)*3) - padding);
	long temp = 0; // Holds decoded quanta

  auto cursor = std::begin(input);
	while(cursor < input.end()) {
		for(size_t quantumPosition = 0; quantumPosition < 4; quantumPosition++) {
			temp <<= 6;
			if(*cursor >= 0x41 && *cursor <= 0x5A) {
				temp |= *cursor - 0x41;
      } else if(*cursor >= 0x61 && *cursor <= 0x7A) {
				temp |= *cursor - 0x47;
      } else if(*cursor >= 0x30 && *cursor <= 0x39) {
				temp |= *cursor + 0x04;
      } else if(*cursor == 0x2B) {
				temp |= 0x3E;
      } else if(*cursor == 0x2F) {
				temp |= 0x3F;
      } else if(*cursor == padCharacter) {
        // Encountered padding
        const unsigned paddingCount = input.end() - cursor;
        if(paddingCount == 1) {
					decodedBytes.push_back((temp >> 16) & 0x000000FF);
					decodedBytes.push_back((temp >> 8 ) & 0x000000FF);
					return decodedBytes;
        } else if(paddingCount == 2) {
					decodedBytes.push_back((temp >> 10) & 0x000000FF);
					return decodedBytes;
        } else {
					throw std::runtime_error("Invalid padding in base 64 encountered");
        }
			} else {
				throw std::runtime_error("Invalid character in base 64 encountered");
      }

			++cursor;
		}

		decodedBytes.push_back((temp >> 16) & 0x000000FF);
		decodedBytes.push_back((temp >> 8 ) & 0x000000FF);
		decodedBytes.push_back((temp      ) & 0x000000FF);
	}

	return decodedBytes;
}

} // namespace base64

} // namespace Scine

#endif
