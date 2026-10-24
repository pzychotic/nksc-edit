#pragma once

#include <bit>
#include <cstddef>


namespace nksc::wb {

static_assert( std::endian::native == std::endian::little, "the white balance blob is little endian" );

// Layout of the binary "Data" blob of the nikon::WhiteBalance filter (packed, little endian).
// Only the fields this tool changes are named; everything else is kept as is.
//
//  offset  type    meaning
//   0..15  int[4]  unknown
//  16      int32   mode (2 = recorded, 4 = auto, see app:WB_method)
//  20      int32   unknown
//  24      int32   color temperature in Kelvin
//  28..39  int[3]  unknown
//  40      char    unknown
//  41      double  image balance (tint)
//  49..50  char[2] unknown
//  version 9 only:
//  51      int32   unknown
//  55..56  char[2] unknown

inline constexpr std::size_t kModeOffset      = 16;
inline constexpr std::size_t kColorTempOffset = 24;
inline constexpr std::size_t kBalanceOffset   = 41;

inline constexpr std::size_t kSizeVersion8 = 51;
inline constexpr std::size_t kSizeVersion9 = 57;

} // namespace nksc::wb
