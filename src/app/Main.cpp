#include "CommandLine.h"

#include <NineEdits.h>
#include <NkscFile.h>

#include <cstdio>
#include <filesystem>
#include <print>


namespace {

int Fail( std::string_view sMessage )
{
    std::println( stderr, "error: {}", sMessage );
    return 1;
}

} // namespace


int main( int argc, const char* argv[] )
{
    nksc::CommandLine kCmdLine;

    const auto& bDump         = kCmdLine.AddFlag(   "dump",         'd',                "Write the NineEdits XML to <file>_NineEdits.xml and exit" );
    const auto& sGain         = kCmdLine.AddOption( "gain",         'g', "value",       "Exposure gain" );
    const auto& sRoll         = kCmdLine.AddOption( "roll",         'r', "degrees",     "Straighten angle in degrees" );
    const auto& sCrop         = kCmdLine.AddOption( "crop",         'c', "x1,y1,x2,y2", "Crop rectangle in pixels" );
    const auto& sColorTemp    = kCmdLine.AddOption( "colortemp",    't', "kelvin",      "White balance color temperature" );
    const auto& sImageBalance = kCmdLine.AddOption( "imagebalance", 'b', "value",       "White balance image balance (tint)" );
    const auto& bHelp         = kCmdLine.AddFlag(   "help",         'h',                "Show this help" );

    const std::string sProgram = std::filesystem::path( argv[0] ).filename().string();
    const std::string sUsage = kCmdLine.Usage( sProgram, "<file.nksc>" );

    if( auto kParsed = kCmdLine.Parse( std::span( argv + 1, static_cast<std::size_t>( argc - 1 ) ) ); !kParsed )
    {
        std::println( stderr, "error: {}\n\n{}", kParsed.error(), sUsage );
        return 1;
    }

    if( argc == 1 || bHelp )
    {
        std::print( "{}", sUsage );
        return 0;
    }

    if( kCmdLine.Positionals().size() != 1 )
    {
        std::println( stderr, "error: expected exactly one .nksc file\n\n{}", sUsage );
        return 1;
    }

    const std::filesystem::path kPath = kCmdLine.Positionals().front();

    auto kFile = nksc::NkscFile::Load( kPath );
    if( !kFile ) return Fail( kFile.error() );

    auto sXml = kFile->NineEditsXml();
    if( !sXml ) return Fail( sXml.error() );

    auto kEdits = nksc::NineEdits::Parse( *sXml );
    if( !kEdits ) return Fail( kEdits.error() );

    if( bDump )
    {
        std::filesystem::path kDumpPath = kPath;
        kDumpPath += "_NineEdits.xml";
        if( auto kWritten = nksc::WriteFile( kDumpPath, kEdits->Dump() ); !kWritten ) return Fail( kWritten.error() );
        std::println( "Wrote {}", kDumpPath.string() );
        return 0;
    }

    if( !sGain && !sRoll && !sColorTemp && !sImageBalance && !sCrop )
    {
        std::println( "Nothing to change." );
        return 0;
    }

    if( sGain )
    {
        if( auto kResult = kEdits->SetGain( *sGain ); !kResult ) return Fail( kResult.error() );
    }

    if( sRoll )
    {
        if( auto kResult = kEdits->SetRoll( *sRoll ); !kResult ) return Fail( kResult.error() );
    }

    if( sColorTemp || sImageBalance )
    {
        std::optional<int> iColorTemp;
        std::optional<double> dBalance;
        if( sColorTemp )
        {
            auto iValue = nksc::ParseInt( *sColorTemp );
            if( !iValue ) return Fail( "colortemp: " + iValue.error() );
            iColorTemp = *iValue;
        }
        if( sImageBalance )
        {
            auto dValue = nksc::ParseDouble( *sImageBalance );
            if( !dValue ) return Fail( "imagebalance: " + dValue.error() );
            dBalance = *dValue;
        }
        if( auto kResult = kEdits->SetWhiteBalance( iColorTemp, dBalance ); !kResult ) return Fail( kResult.error() );
    }

    if( sCrop )
    {
        auto kRect = nksc::ParseCrop( *sCrop );
        if( !kRect ) return Fail( kRect.error() );
        if( auto kResult = kEdits->SetCrop( *kRect ); !kResult ) return Fail( kResult.error() );
    }

    kFile->SetNineEditsXml( kEdits->Print() );
    if( auto kSaved = kFile->Save( kPath ); !kSaved ) return Fail( kSaved.error() );

    return 0;
}
