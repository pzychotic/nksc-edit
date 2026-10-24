#include "XmlEscape.h"

#include <charconv>
#include <cstdint>
#include <format>


namespace nksc::xml {

namespace {

void AppendUtf8( std::string& sOut, std::uint32_t uCodePoint )
{
    if( uCodePoint < 0x80 )
    {
        sOut += static_cast<char>( uCodePoint );
    }
    else if( uCodePoint < 0x800 )
    {
        sOut += static_cast<char>( 0xC0 | ( uCodePoint >> 6 ) );
        sOut += static_cast<char>( 0x80 | ( uCodePoint & 0x3F ) );
    }
    else if( uCodePoint < 0x10000 )
    {
        sOut += static_cast<char>( 0xE0 | ( uCodePoint >> 12 ) );
        sOut += static_cast<char>( 0x80 | ( ( uCodePoint >> 6 ) & 0x3F ) );
        sOut += static_cast<char>( 0x80 | ( uCodePoint & 0x3F ) );
    }
    else
    {
        sOut += static_cast<char>( 0xF0 | ( uCodePoint >> 18 ) );
        sOut += static_cast<char>( 0x80 | ( ( uCodePoint >> 12 ) & 0x3F ) );
        sOut += static_cast<char>( 0x80 | ( ( uCodePoint >> 6 ) & 0x3F ) );
        sOut += static_cast<char>( 0x80 | ( uCodePoint & 0x3F ) );
    }
}

} // namespace


std::string Escape( std::string_view sText )
{
    std::string sResult;
    sResult.reserve( sText.size() + sText.size() / 4 );

    for( char c : sText )
    {
        switch( c )
        {
        case '&': sResult += "&amp;"; break;
        case '<': sResult += "&lt;";  break;
        case '>': sResult += "&gt;";  break;
        default:
            // control characters (e.g. the line breaks in base64 data) as character references
            if( static_cast< unsigned char >( c ) < 0x20 )
            {
                sResult += std::format( "&#x{:X};", static_cast< unsigned char >( c ) );
            }
            else
            {
                sResult += c;
            }
            break;
        }
    }

    return sResult;
}


std::expected<std::string, std::string> Unescape( std::string_view sText )
{
    std::string sResult;
    sResult.reserve( sText.size() );

    std::size_t uPos = 0;
    while( uPos < sText.size() )
    {
        const std::size_t uAmp = sText.find( '&', uPos );
        sResult += sText.substr( uPos, uAmp - uPos );
        if( uAmp == std::string_view::npos )
        {
            break;
        }

        const std::size_t uSemi = sText.find( ';', uAmp );
        if( uSemi == std::string_view::npos )
        {
            return std::unexpected( "unterminated entity at offset " + std::to_string( uAmp ) );
        }

        const std::string_view sEntity = sText.substr( uAmp + 1, uSemi - uAmp - 1 );

        if(      sEntity == "lt" )   sResult += '<';
        else if( sEntity == "gt" )   sResult += '>';
        else if( sEntity == "amp" )  sResult += '&';
        else if( sEntity == "quot" ) sResult += '"';
        else if( sEntity == "apos" ) sResult += '\'';
        else if( sEntity.starts_with( '#' ) )
        {
            const bool bHex = sEntity.size() > 1 && ( sEntity[1] == 'x' || sEntity[1] == 'X' );
            const std::string_view sDigits = sEntity.substr( bHex ? 2 : 1 );

            std::uint32_t uCodePoint = 0;
            const auto [pEnd, eError] = std::from_chars( sDigits.data(), sDigits.data() + sDigits.size(), uCodePoint, bHex ? 16 : 10 );
            if( sDigits.empty() || eError != std::errc{} || pEnd != sDigits.data() + sDigits.size() || uCodePoint > 0x10FFFF )
            {
                return std::unexpected( "invalid character reference '&" + std::string( sEntity ) + ";'" );
            }
            AppendUtf8( sResult, uCodePoint );
        }
        else
        {
            return std::unexpected( "unknown entity '&" + std::string( sEntity ) + ";'" );
        }

        uPos = uSemi + 1;
    }

    return sResult;
}

} // namespace nksc::xml
