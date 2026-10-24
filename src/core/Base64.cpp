#include "Base64.h"

#include <algorithm>
#include <array>
#include <cstdint>


namespace nksc::base64 {

namespace {

constexpr std::string_view s_sAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::array<std::int8_t, 256> MakeDecodeTable()
{
    std::array<std::int8_t, 256> aTable{};
    aTable.fill( -1 );
    for( std::size_t i = 0; i < s_sAlphabet.size(); ++i )
    {
        aTable[static_cast<unsigned char>( s_sAlphabet[i] )] = static_cast<std::int8_t>( i );
    }
    return aTable;
}

constexpr auto s_aDecodeTable = MakeDecodeTable();

bool IsWhitespace( char c )
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

} // namespace


std::string Encode( std::span<const std::byte> aData, std::size_t uLineLength )
{
    std::string sChars;
    sChars.reserve( ( aData.size() + 2 ) / 3 * 4 );

    for( std::size_t i = 0; i < aData.size(); i += 3 )
    {
        const std::size_t uCount = std::min<std::size_t>( 3, aData.size() - i );

        std::uint32_t uBits = std::to_integer<std::uint32_t>( aData[i] ) << 16;
        if( uCount > 1 ) uBits |= std::to_integer<std::uint32_t>( aData[i + 1] ) << 8;
        if( uCount > 2 ) uBits |= std::to_integer<std::uint32_t>( aData[i + 2] );

        sChars += s_sAlphabet[( uBits >> 18 ) & 0x3F];
        sChars += s_sAlphabet[( uBits >> 12 ) & 0x3F];
        sChars += uCount > 1 ? s_sAlphabet[( uBits >> 6 ) & 0x3F] : '=';
        sChars += uCount > 2 ? s_sAlphabet[uBits & 0x3F] : '=';
    }

    if( uLineLength == 0 )
    {
        return sChars;
    }

    std::string sResult;
    sResult.reserve( sChars.size() + sChars.size() / uLineLength + 1 );
    for( std::size_t i = 0; i < sChars.size(); i += uLineLength )
    {
        sResult += std::string_view( sChars ).substr( i, uLineLength );
        sResult += '\n';
    }
    return sResult;
}


std::expected<std::vector<std::byte>, std::string> Decode( std::string_view sText )
{
    std::vector<std::byte> aResult;
    aResult.reserve( sText.size() / 4 * 3 );

    std::uint32_t uBits = 0;
    int iBitCount = 0;
    int iPadding = 0;

    for( char c : sText )
    {
        if( IsWhitespace( c ) )
        {
            continue;
        }
        if( c == '=' )
        {
            ++iPadding;
            continue;
        }
        if( iPadding > 0 )
        {
            return std::unexpected( "base64: data after padding" );
        }

        const std::int8_t iValue = s_aDecodeTable[static_cast<unsigned char>( c )];
        if( iValue < 0 )
        {
            return std::unexpected( std::string( "base64: invalid character '" ) + c + "'" );
        }

        uBits = ( uBits << 6 ) | static_cast<std::uint32_t>( iValue );
        iBitCount += 6;
        if( iBitCount >= 8 )
        {
            iBitCount -= 8;
            aResult.push_back( static_cast<std::byte>( ( uBits >> iBitCount ) & 0xFF ) );
        }
    }

    if( iPadding > 2 || iBitCount >= 6 )
    {
        return std::unexpected( "base64: invalid length" );
    }

    return aResult;
}

} // namespace nksc::base64
