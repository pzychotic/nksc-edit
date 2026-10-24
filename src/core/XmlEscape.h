#pragma once

#include <expected>
#include <string>
#include <string_view>


namespace nksc::xml {

// Escapes '&', '<', '>' and control characters ('\n' -> "&#xA;"), quotes stay literal.
// This matches what NX Studio writes.
std::string Escape( std::string_view sText );

// Resolves the five predefined entities and numeric character references (&#N; / &#xN;).
std::expected<std::string, std::string> Unescape( std::string_view sText );

} // namespace nksc::xml
