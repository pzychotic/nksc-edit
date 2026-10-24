#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>


namespace nksc {

// An NX Studio sidecar file (.nksc). The file is an XMP packet, but the only part this
// tool touches is the text of the <nine:NineEdits> element, an escaped XML document.
// Everything before and after that text is kept byte for byte.
class NkscFile
{
public:

    static std::expected<NkscFile, std::string> Load( const std::filesystem::path& kPath );
    static std::expected<NkscFile, std::string> FromString( std::string sContent );

    std::expected<void, std::string> Save( const std::filesystem::path& kPath ) const;

    // The unescaped NineEdits XML document.
    std::expected<std::string, std::string> NineEditsXml() const;
    void SetNineEditsXml( std::string_view sXml );

    const std::string& Content() const { return m_sContent; }

    // The raw (still escaped) NineEdits text and the parts of the file around it.
    std::string_view Prefix()  const { return std::string_view( m_sContent ).substr( 0, m_uBegin ); }
    std::string_view Payload() const { return std::string_view( m_sContent ).substr( m_uBegin, m_uEnd - m_uBegin ); }
    std::string_view Suffix()  const { return std::string_view( m_sContent ).substr( m_uEnd ); }

private:

    std::string m_sContent;
    std::size_t m_uBegin = 0;   // offset of the NineEdits text
    std::size_t m_uEnd   = 0;   // offset of the closing tag
};


std::expected<std::string, std::string> ReadFile( const std::filesystem::path& kPath );
std::expected<void, std::string> WriteFile( const std::filesystem::path& kPath, std::string_view sContent );

} // namespace nksc
