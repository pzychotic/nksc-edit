#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>


namespace nksc::base64 {

// Encodes binary data the way NX Studio writes it: lines of 60 characters,
// each line (including the last one) terminated by '\n'.
// A line length of 0 disables line breaks.
std::string Encode( std::span<const std::byte> aData, std::size_t uLineLength = 60 );

// Decodes base64 text, whitespace (line breaks) is ignored.
std::expected<std::vector<std::byte>, std::string> Decode( std::string_view sText );

} // namespace nksc::base64
