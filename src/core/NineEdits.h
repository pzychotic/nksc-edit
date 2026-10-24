#pragma once

#include <tinyxml2/tinyxml2.h>

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>


namespace nksc {

struct CropRect
{
    int iX1 = 0;
    int iY1 = 0;
    int iX2 = 0;
    int iY2 = 0;
};

// Parses "x1,y1,x2,y2".
std::expected<CropRect, std::string> ParseCrop( std::string_view sText );

std::expected<int, std::string>     ParseInt( std::string_view sText );
std::expected<double, std::string>  ParseDouble( std::string_view sText );


// The NineEdits XML document (<userData> with one <filter> per NX Studio adjustment).
class NineEdits
{
public:

    static std::expected<NineEdits, std::string> Parse( std::string_view sXml );

    // Compact output, as written back into the .nksc file.
    std::string Print() const;

    // Indented output with CRLF line endings for dumping into a readable file.
    std::string Dump() const;

    std::expected<void, std::string> SetGain( std::string_view sValue );
    std::expected<void, std::string> SetRoll( std::string_view sDegrees );
    std::expected<void, std::string> SetCrop( const CropRect& kRect );
    std::expected<void, std::string> SetWhiteBalance( std::optional<int> iColorTemp, std::optional<double> dBalance );

    tinyxml2::XMLElement* Root() const { return m_pDoc->FirstChildElement( "userData" ); }

    // Returns the top level <filter id="..."> element or nullptr.
    tinyxml2::XMLElement* FindFilter( std::string_view sId ) const;

private:

    std::expected<tinyxml2::XMLElement*, std::string> GetHistoryFilterParams( const char* sFilterId );

    // XMLDocument is neither copyable nor movable.
    std::unique_ptr<tinyxml2::XMLDocument> m_pDoc;
};


// Filter ids of the history steps NX Studio always writes, in this fixed order.
inline constexpr const char* s_aHistoryFilterIds[] =
{
    "nik::AdaptivePaste",
    "nik::Newton",
    "nik::Straighten",
    "nik::Rotate",
    "nik::Crop"
};

} // namespace nksc
