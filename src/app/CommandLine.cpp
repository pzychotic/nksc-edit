#include "CommandLine.h"

#include <algorithm>
#include <format>


namespace nksc {

const bool& CommandLine::AddFlag( std::string_view sLongName, char cShortName, std::string_view sHelp )
{
    Argument& rArg = m_aArguments.emplace_back( std::string( sLongName ), cShortName, std::string(), std::string( sHelp ), false, std::nullopt );
    return rArg.bSet;
}


const std::optional<std::string>& CommandLine::AddOption( std::string_view sLongName, char cShortName, std::string_view sValueName, std::string_view sHelp )
{
    Argument& rArg = m_aArguments.emplace_back( std::string( sLongName ), cShortName, std::string( sValueName ), std::string( sHelp ), false, std::nullopt );
    return rArg.sValue;
}


CommandLine::Argument* CommandLine::Find( std::string_view sLongName, char cShortName )
{
    auto it = std::ranges::find_if( m_aArguments, [&]( const Argument& rArg ) {
        return sLongName.empty() ? rArg.cShortName == cShortName : rArg.sLongName == sLongName;
    } );
    return it != m_aArguments.end() ? &*it : nullptr;
}


std::expected<void, std::string> CommandLine::Parse( std::span<const char* const> aArgs )
{
    bool bOptionsEnded = false;

    for( std::size_t i = 0; i < aArgs.size(); ++i )
    {
        const std::string_view sArg = aArgs[i];

        if( bOptionsEnded || !sArg.starts_with( '-' ) || sArg == "-" )
        {
            m_aPositionals.emplace_back( sArg );
            continue;
        }
        if( sArg == "--" )
        {
            bOptionsEnded = true;
            continue;
        }

        std::string_view sName;
        std::optional<std::string_view> sInlineValue;
        Argument* pArg = nullptr;

        if( sArg.starts_with( "--" ) )
        {
            sName = sArg.substr( 2 );
            if( const std::size_t uEq = sName.find( '=' ); uEq != std::string_view::npos )
            {
                sInlineValue = sName.substr( uEq + 1 );
                sName = sName.substr( 0, uEq );
            }
            pArg = Find( sName, 0 );
        }
        else if( sArg.size() == 2 )
        {
            sName = sArg.substr( 1 );
            pArg = Find( {}, sArg[1] );
        }

        if( pArg == nullptr )
        {
            return std::unexpected( std::format( "unknown option '{}'", sArg ) );
        }

        if( pArg->sValueName.empty() )
        {
            if( sInlineValue )
            {
                return std::unexpected( std::format( "option '{}' takes no value", sArg ) );
            }
            pArg->bSet = true;
        }
        else if( sInlineValue )
        {
            pArg->sValue = std::string( *sInlineValue );
        }
        else if( i + 1 < aArgs.size() )
        {
            pArg->sValue = aArgs[++i];
        }
        else
        {
            return std::unexpected( std::format( "option '{}' requires a value", sArg ) );
        }
    }

    return {};
}


std::string CommandLine::Usage( std::string_view sProgramName, std::string_view sPositionalUsage ) const
{
    std::string sUsage = std::format( "Usage: {} [options] {}\n\nOptions:\n", sProgramName, sPositionalUsage );

    std::vector<std::string> aNames;
    std::size_t uWidth = 0;
    for( const Argument& rArg : m_aArguments )
    {
        std::string sName = std::format( "-{}, --{}", rArg.cShortName, rArg.sLongName );
        if( !rArg.sValueName.empty() )
        {
            sName += std::format( " <{}>", rArg.sValueName );
        }
        uWidth = std::max( uWidth, sName.size() );
        aNames.push_back( std::move( sName ) );
    }

    for( std::size_t i = 0; i < m_aArguments.size(); ++i )
    {
        sUsage += std::format( "  {:<{}}  {}\n", aNames[i], uWidth, m_aArguments[i].sHelp );
    }

    return sUsage;
}

} // namespace nksc
