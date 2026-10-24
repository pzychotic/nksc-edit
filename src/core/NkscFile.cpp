#include "NkscFile.h"

#include "XmlEscape.h"

#include <fstream>
#include <sstream>


namespace nksc {

namespace {

constexpr std::string_view s_sOpenTag  = "<nine:NineEdits>";
constexpr std::string_view s_sCloseTag = "</nine:NineEdits>";
constexpr std::string_view s_sAttrForm = "nine:NineEdits=";

} // namespace


std::expected<std::string, std::string> ReadFile( const std::filesystem::path& kPath )
{
    std::ifstream kFile( kPath, std::ios::in | std::ios::binary );
    if( !kFile )
    {
        return std::unexpected( "could not open file: " + kPath.string() );
    }

    std::ostringstream kContent;
    kContent << kFile.rdbuf();
    return std::move( kContent ).str();
}


std::expected<void, std::string> WriteFile( const std::filesystem::path& kPath, std::string_view sContent )
{
    std::ofstream kFile( kPath, std::ios::out | std::ios::binary | std::ios::trunc );
    if( !kFile.write( sContent.data(), static_cast<std::streamsize>( sContent.size() ) ) )
    {
        return std::unexpected( "could not write file: " + kPath.string() );
    }
    return {};
}


std::expected<NkscFile, std::string> NkscFile::Load( const std::filesystem::path& kPath )
{
    auto sContent = ReadFile( kPath );
    if( !sContent )
    {
        return std::unexpected( sContent.error() );
    }
    if( sContent->empty() )
    {
        return std::unexpected( "file is empty: " + kPath.string() );
    }
    return FromString( std::move( *sContent ) );
}


std::expected<NkscFile, std::string> NkscFile::FromString( std::string sContent )
{
    const std::size_t uOpen = sContent.find( s_sOpenTag );
    if( uOpen == std::string::npos )
    {
        if( sContent.find( s_sAttrForm ) != std::string::npos )
        {
            return std::unexpected( "nine:NineEdits is stored as attribute, which is not supported" );
        }
        return std::unexpected( "nine:NineEdits entry not found" );
    }

    const std::size_t uBegin = uOpen + s_sOpenTag.size();
    const std::size_t uEnd = sContent.find( s_sCloseTag, uBegin );
    if( uEnd == std::string::npos )
    {
        return std::unexpected( "closing </nine:NineEdits> tag not found" );
    }

    NkscFile kFile;
    kFile.m_sContent = std::move( sContent );
    kFile.m_uBegin = uBegin;
    kFile.m_uEnd = uEnd;
    return kFile;
}


std::expected<void, std::string> NkscFile::Save( const std::filesystem::path& kPath ) const
{
    return WriteFile( kPath, m_sContent );
}


std::expected<std::string, std::string> NkscFile::NineEditsXml() const
{
    return xml::Unescape( Payload() );
}


void NkscFile::SetNineEditsXml( std::string_view sXml )
{
    const std::string sEscaped = xml::Escape( sXml );
    m_sContent.replace( m_uBegin, m_uEnd - m_uBegin, sEscaped );
    m_uEnd = m_uBegin + sEscaped.size();
}

} // namespace nksc
