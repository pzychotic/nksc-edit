// Self-contained tests for nksc-edit, no test framework.
// Usage: nksc-edit-tests [data dir]   (defaults to "data" in the current directory)

#include "CommandLine.h"

#include <Base64.h>
#include <NineEdits.h>
#include <NkscFile.h>
#include <WhiteBalance.h>
#include <XmlEscape.h>

#include <cstring>
#include <filesystem>
#include <functional>
#include <print>
#include <regex>
#include <source_location>
#include <string>
#include <vector>


namespace fs = std::filesystem;
using tinyxml2::XMLElement;


// --- minimal test harness ---------------------------------------------------

namespace {

int s_iChecks = 0;
int s_iFailures = 0;
std::string s_sCurrentTest;
fs::path s_kDataDir = "data";

bool Check( bool bOk, const char* sExpr, std::source_location kLoc = std::source_location::current() )
{
    ++s_iChecks;
    if( !bOk )
    {
        ++s_iFailures;
        std::println( stderr, "{}({}): FAILED in '{}': {}", kLoc.file_name(), kLoc.line(), s_sCurrentTest, sExpr );
    }
    return bOk;
}

#define CHECK( ... ) Check( static_cast<bool>( __VA_ARGS__ ), #__VA_ARGS__ )

// Checks an std::expected, prints its error and returns from the test if it failed.
#define REQUIRE( ... )                                                                         \
    if( auto&& kRequired_ = ( __VA_ARGS__ ); !Check( kRequired_.has_value(), #__VA_ARGS__ ) )  \
    {                                                                                          \
        std::println( stderr, "    error: {}", kRequired_.error() );                           \
        return;                                                                                \
    }

struct Test
{
    const char* sName;
    std::function<void()> fnRun;
};

std::vector<Test>& Registry()
{
    static std::vector<Test> s_aTests;
    return s_aTests;
}

struct Register
{
    Register( const char* sName, std::function<void()> fnRun ) { Registry().push_back( { sName, std::move( fnRun ) } ); }
};

#define TEST( name )                                   \
    void name();                                       \
    const Register s_kRegister_##name( #name, name );  \
    void name()


// --- helpers ------------------------------------------------------------------

std::vector<fs::path> NkscFiles()
{
    std::vector<fs::path> aFiles;
    for( const auto& kEntry : fs::directory_iterator( s_kDataDir ) )
    {
        if( kEntry.path().extension() == ".nksc" )
        {
            aFiles.push_back( kEntry.path() );
        }
    }
    std::ranges::sort( aFiles );
    return aFiles;
}

// Older versions of this tool wrote empty <binary .../> elements self-closing (Test_02),
// NX Studio and the current version write <binary ...></binary>. Both are the same XML.
std::string NormalizeEmptyBinary( const std::string& sXml )
{
    static const std::regex s_kSelfClosing( R"(<binary( [^<>]*)/>)" );
    return std::regex_replace( sXml, s_kSelfClosing, "<binary$1></binary>" );
}

std::vector<std::byte> Bytes( std::string_view s )
{
    std::vector<std::byte> aBytes( s.size() );
    std::memcpy( aBytes.data(), s.data(), s.size() );
    return aBytes;
}

XMLElement* FindChild( XMLElement* pParent, const char* sTag, const char* sAttr, std::string_view sValue )
{
    for( XMLElement* p = pParent ? pParent->FirstChildElement( sTag ) : nullptr; p != nullptr; p = p->NextSiblingElement( sTag ) )
    {
        if( p->Attribute( sAttr ) != nullptr && sValue == p->Attribute( sAttr ) )
        {
            return p;
        }
    }
    return nullptr;
}

// All <filter> elements of the NXHistory steps, in document order.
std::vector<XMLElement*> HistoryFilters( const nksc::NineEdits& kEdits )
{
    std::vector<XMLElement*> aFilters;
    XMLElement* pHistory = kEdits.FindFilter( "nikon::NXHistory" );
    XMLElement* pParams = pHistory ? pHistory->FirstChildElement( "parameters" ) : nullptr;
    for( XMLElement* pStep = pParams ? pParams->FirstChildElement( "historystep" ) : nullptr; pStep != nullptr; pStep = pStep->NextSiblingElement( "historystep" ) )
    {
        aFilters.push_back( pStep->FirstChildElement( "filter" ) );
    }
    return aFilters;
}

XMLElement* HistoryFilter( const nksc::NineEdits& kEdits, std::string_view sId )
{
    for( XMLElement* pFilter : HistoryFilters( kEdits ) )
    {
        if( pFilter != nullptr && sId == pFilter->Attribute( "id" ) )
        {
            return pFilter;
        }
    }
    return nullptr;
}

std::expected<std::vector<std::byte>, std::string> WhiteBalanceBlob( const nksc::NineEdits& kEdits )
{
    XMLElement* pBinary = kEdits.FindFilter( "nikon::WhiteBalance" )->FirstChildElement( "parameters" )->FirstChildElement( "binary" );
    return nksc::base64::Decode( pBinary->GetText() );
}

template<typename T>
T ReadAt( const std::vector<std::byte>& aData, std::size_t uOffset )
{
    T value{};
    std::memcpy( &value, aData.data() + uOffset, sizeof( T ) );
    return value;
}

// Applies fnEdit to a copy of the file in the temp dir, saves it and loads it again.
template<typename Fn>
void EditRoundTrip( const fs::path& kSource, Fn fnEdit, std::function<void( const nksc::NkscFile&, const nksc::NineEdits& )> fnVerify )
{
    const fs::path kCopy = fs::temp_directory_path() / ( "nksc-edit-tests_" + kSource.filename().string() );
    fs::copy_file( kSource, kCopy, fs::copy_options::overwrite_existing );

    {
        auto kFile = nksc::NkscFile::Load( kCopy );
        REQUIRE( kFile );
        auto sXml = kFile->NineEditsXml();
        REQUIRE( sXml );
        auto kEdits = nksc::NineEdits::Parse( *sXml );
        REQUIRE( kEdits );
        REQUIRE( fnEdit( *kEdits ) );
        kFile->SetNineEditsXml( kEdits->Print() );
        REQUIRE( kFile->Save( kCopy ) );
    }

    auto kOriginal = nksc::NkscFile::Load( kSource );
    auto kFile = nksc::NkscFile::Load( kCopy );
    fs::remove( kCopy );
    REQUIRE( kOriginal );
    REQUIRE( kFile );

    // only the NineEdits text may change
    CHECK( kFile->Prefix() == kOriginal->Prefix() );
    CHECK( kFile->Suffix() == kOriginal->Suffix() );
    CHECK( kFile->Payload() != kOriginal->Payload() );

    auto sXml = kFile->NineEditsXml();
    REQUIRE( sXml );
    auto kEdits = nksc::NineEdits::Parse( *sXml );
    REQUIRE( kEdits );
    fnVerify( *kFile, *kEdits );
}

} // namespace


// --- base64 -------------------------------------------------------------------

TEST( Base64_Rfc4648Vectors )
{
    const std::pair<std::string_view, std::string_view> aVectors[] =
    {
        { "", "" }, { "f", "Zg==" }, { "fo", "Zm8=" }, { "foo", "Zm9v" },
        { "foob", "Zm9vYg==" }, { "fooba", "Zm9vYmE=" }, { "foobar", "Zm9vYmFy" },
    };

    for( const auto& [sPlain, sEncoded] : aVectors )
    {
        CHECK( nksc::base64::Encode( Bytes( sPlain ), 0 ) == sEncoded );
        auto aDecoded = nksc::base64::Decode( sEncoded );
        REQUIRE( aDecoded );
        CHECK( *aDecoded == Bytes( sPlain ) );
    }
}

TEST( Base64_LineBreaks )
{
    const std::vector<std::byte> aData( 57, std::byte{ 0xAB } );
    const std::string sEncoded = nksc::base64::Encode( aData );

    // 76 characters: one full line of 60 and one of 16, each terminated by '\n'
    CHECK( sEncoded.size() == 78 );
    CHECK( sEncoded[60] == '\n' );
    CHECK( sEncoded.back() == '\n' );

    auto aDecoded = nksc::base64::Decode( sEncoded );
    REQUIRE( aDecoded );
    CHECK( *aDecoded == aData );

    CHECK( nksc::base64::Decode( "Zm9v\r\nYmFy" ).value_or( std::vector<std::byte>{} ) == Bytes( "foobar" ) );
}

TEST( Base64_Errors )
{
    CHECK( !nksc::base64::Decode( "Zm9v!" ) );
    CHECK( !nksc::base64::Decode( "Zm9vY" ) );
    CHECK( !nksc::base64::Decode( "Zg==Zg==" ) );
}


// --- XML escaping -------------------------------------------------------------

TEST( XmlEscape_RoundTrip )
{
    const std::string_view sText = R"(<a id="x">Tom & 'Jerry'</a>)";
    const std::string sEscaped = nksc::xml::Escape( sText );
    CHECK( sEscaped == R"(&lt;a id="x"&gt;Tom &amp; 'Jerry'&lt;/a&gt;)" );
    CHECK( nksc::xml::Unescape( sEscaped ).value_or( "" ) == sText );
}

TEST( XmlEscape_Entities )
{
    CHECK( nksc::xml::Unescape( "&quot;&apos;&#65;&#x42;&#xA;&#xE4;" ).value_or( "" ) == "\"'AB\n\xC3\xA4" );
    CHECK( !nksc::xml::Unescape( "&unknown;" ) );
    CHECK( !nksc::xml::Unescape( "a & b" ) );
    CHECK( !nksc::xml::Unescape( "&#xZZ;" ) );
    CHECK( !nksc::xml::Unescape( "end &" ) );
}


// --- command line -------------------------------------------------------------

TEST( CommandLine_Forms )
{
    nksc::CommandLine kCmdLine;
    const auto& bDump = kCmdLine.AddFlag( "dump", 'd', "" );
    const auto& bHelp = kCmdLine.AddFlag( "help", 'h', "" );
    const auto& sGain = kCmdLine.AddOption( "gain", 'g', "value", "" );
    const auto& sRoll = kCmdLine.AddOption( "roll", 'r', "degrees", "" );
    const auto& sCrop = kCmdLine.AddOption( "crop", 'c', "rect", "" );
    const auto& sTemp = kCmdLine.AddOption( "colortemp", 't', "kelvin", "" );

    const char* aArgs[] = { "-d", "--gain", "0.5", "--roll=-2", "file.nksc", "-c", "1,2,3,4", "--", "-odd.nksc" };
    REQUIRE( kCmdLine.Parse( aArgs ) );

    CHECK( bDump );
    CHECK( !bHelp );
    CHECK( sGain == "0.5" );
    CHECK( sRoll == "-2" );
    CHECK( sCrop == "1,2,3,4" );
    CHECK( !sTemp );
    CHECK( kCmdLine.Positionals() == std::vector<std::string>{ "file.nksc", "-odd.nksc" } );
    CHECK( kCmdLine.Usage( "nksc-edit", "<file>" ).contains( "-t, --colortemp <kelvin>" ) );
}

TEST( CommandLine_Errors )
{
    auto Parse = []( std::vector<const char*> aArgs )
    {
        nksc::CommandLine kCmdLine;
        kCmdLine.AddFlag( "dump", 'd', "" );
        kCmdLine.AddOption( "gain", 'g', "value", "" );
        return kCmdLine.Parse( aArgs );
    };

    CHECK( !Parse( { "--unknown" } ) );
    CHECK( !Parse( { "-x" } ) );
    CHECK( !Parse( { "-ct", "5000" } ) );
    CHECK( !Parse( { "--gain" } ) );
    CHECK( !Parse( { "--dump=1" } ) );
    CHECK( Parse( { "-g", "-1" } ) ); // a value may start with '-'
}

TEST( ParseCrop )
{
    auto kRect = nksc::ParseCrop( "10,-1,500,400" );
    REQUIRE( kRect );
    CHECK( kRect->iX1 == 10 );
    CHECK( kRect->iY1 == -1 );
    CHECK( kRect->iX2 == 500 );
    CHECK( kRect->iY2 == 400 );

    CHECK( !nksc::ParseCrop( "1,2,3" ) );
    CHECK( !nksc::ParseCrop( "1,2,3,4,5" ) );
    CHECK( !nksc::ParseCrop( "1,2,x,4" ) );
    CHECK( !nksc::ParseCrop( "1,,3,4" ) );
    CHECK( !nksc::ParseInt( "12a" ) );
    CHECK( !nksc::ParseDouble( "" ) );
}


// --- sample files -------------------------------------------------------------

TEST( Data_FilesPresent )
{
    CHECK( NkscFiles().size() >= 3 );
}

TEST( Data_UnchangedRoundTripIsLossless )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        std::println( "    {}", kPath.filename().string() );

        auto kFile = nksc::NkscFile::Load( kPath );
        REQUIRE( kFile );
        const std::string sOriginal = kFile->Content();

        auto sXml = kFile->NineEditsXml();
        REQUIRE( sXml );

        // escaping
        CHECK( nksc::xml::Escape( *sXml ) == kFile->Payload() );

        // tinyxml2 parse + compact print
        auto kEdits = nksc::NineEdits::Parse( *sXml );
        REQUIRE( kEdits );
        const std::string sPrinted = kEdits->Print();
        const std::string sExpected = NormalizeEmptyBinary( *sXml );
        if( !CHECK( sPrinted == sExpected ) )
        {
            const auto [itA, itB] = std::ranges::mismatch( sPrinted, sExpected );
            const std::size_t uOffset = static_cast<std::size_t>( itA - sPrinted.begin() );
            const std::size_t uStart = uOffset > 40 ? uOffset - 40 : 0;
            std::println( stderr, "    first difference at offset {}\n    printed:  {:?}\n    expected: {:?}", uOffset,
                          sPrinted.substr( uStart, 100 ), sExpected.substr( uStart, 100 ) );
        }

        // files written by NX Studio come out byte identical
        kFile->SetNineEditsXml( sPrinted );
        if( sExpected == *sXml )
        {
            CHECK( kFile->Content() == sOriginal );
        }
    }
}

TEST( Data_DumpMatchesReference )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        fs::path kReference = kPath;
        kReference += "_NineEdits.xml";
        if( !fs::exists( kReference ) )
        {
            continue;
        }

        auto sReference = nksc::ReadFile( kReference );
        REQUIRE( sReference );
        auto kFile = nksc::NkscFile::Load( kPath );
        REQUIRE( kFile );
        auto kEdits = nksc::NineEdits::Parse( kFile->NineEditsXml().value_or( "" ) );
        REQUIRE( kEdits );

        CHECK( kEdits->Dump() == NormalizeEmptyBinary( *sReference ) );
    }
}

TEST( Data_WhiteBalanceBlob )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        auto kFile = nksc::NkscFile::Load( kPath );
        REQUIRE( kFile );
        auto kEdits = nksc::NineEdits::Parse( kFile->NineEditsXml().value_or( "" ) );
        REQUIRE( kEdits );

        XMLElement* pParams = kEdits->FindFilter( "nikon::WhiteBalance" )->FirstChildElement( "parameters" );
        const std::string_view sText = pParams->FirstChildElement( "binary" )->GetText();
        const int iVersion = pParams->FirstChildElement( "integer" )->IntText();

        auto aData = nksc::base64::Decode( sText );
        REQUIRE( aData );
        CHECK( aData->size() == ( iVersion == 8 ? nksc::wb::kSizeVersion8 : nksc::wb::kSizeVersion9 ) );
        CHECK( nksc::base64::Encode( *aData ) == sText ); // NX Studio line format
    }
}

TEST( Edit_Gain )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        EditRoundTrip( kPath,
            []( nksc::NineEdits& k ) { return k.SetGain( "-0.67" ); },
            []( const nksc::NkscFile&, const nksc::NineEdits& k )
            {
                XMLElement* pGain = k.FindFilter( "nikon::ExposureSettings" )->FirstChildElement( "parameters" )->FirstChildElement( "Gain" );
                CHECK( std::string_view( pGain->GetText() ) == "-0.67" );
            } );
    }
}

TEST( Edit_Roll )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        EditRoundTrip( kPath,
            []( nksc::NineEdits& k ) { return k.SetRoll( "2.5" ); },
            []( const nksc::NkscFile&, const nksc::NineEdits& k )
            {
                // all history steps present once, in NX Studio order
                std::vector<std::string> aIds;
                for( XMLElement* pFilter : HistoryFilters( k ) )
                {
                    aIds.emplace_back( pFilter ? pFilter->Attribute( "id" ) : "" );
                }
                CHECK( aIds == std::vector<std::string>( std::begin( nksc::s_aHistoryFilterIds ), std::end( nksc::s_aHistoryFilterIds ) ) );

                XMLElement* pFilter = HistoryFilter( k, "nik::Straighten" );
                if( !CHECK( pFilter != nullptr ) ) return;
                CHECK( std::string_view( pFilter->FirstChildElement( "active" )->GetText() ) == "true" );
                CHECK( std::string_view( FindChild( pFilter->FirstChildElement( "parameters" ), "double", "name", "rotateAngle" )->GetText() ) == "2.5" );
            } );
    }
}

TEST( Edit_Crop )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        EditRoundTrip( kPath,
            []( nksc::NineEdits& k ) { return k.SetCrop( { 10, 20, 500, 400 } ); },
            []( const nksc::NkscFile&, const nksc::NineEdits& k )
            {
                XMLElement* pFilter = HistoryFilter( k, "nik::Crop" );
                if( !CHECK( pFilter != nullptr ) ) return;
                CHECK( std::string_view( pFilter->FirstChildElement( "active" )->GetText() ) == "true" );
                XMLElement* pParams = pFilter->FirstChildElement( "parameters" );
                XMLElement* pStart = FindChild( pParams, "point", "name", "cropStart" );
                XMLElement* pEnd = FindChild( pParams, "point", "name", "cropEnd" );
                CHECK( pStart->IntAttribute( "x" ) == 10 );
                CHECK( pStart->IntAttribute( "y" ) == 20 );
                CHECK( pEnd->IntAttribute( "x" ) == 500 );
                CHECK( pEnd->IntAttribute( "y" ) == 400 );
            } );
    }
}

TEST( Edit_WhiteBalance )
{
    for( const fs::path& kPath : NkscFiles() )
    {
        auto kOriginal = nksc::NkscFile::Load( kPath );
        REQUIRE( kOriginal );
        auto kOriginalEdits = nksc::NineEdits::Parse( kOriginal->NineEditsXml().value_or( "" ) );
        REQUIRE( kOriginalEdits );
        auto aOriginalBlob = WhiteBalanceBlob( *kOriginalEdits );
        REQUIRE( aOriginalBlob );

        EditRoundTrip( kPath,
            []( nksc::NineEdits& k ) { return k.SetWhiteBalance( 5200, 3.25 ); },
            [ & ]( const nksc::NkscFile&, const nksc::NineEdits& k )
            {
                XMLElement* pAppParams = k.FindFilter( "nikon::ApplicationData" )->FirstChildElement( "parameters" );
                XMLElement* pMethod = FindChild( pAppParams, "data", "id", "app:WB_method" );
                CHECK( pMethod != nullptr && std::string_view( pMethod->GetText() ) == "NKD_WB_RECORDED_AND_EDIT" );

                auto aBlob = WhiteBalanceBlob( k );
                REQUIRE( aBlob );
                if( !CHECK( aBlob->size() == aOriginalBlob->size() ) ) return;
                CHECK( ReadAt<std::int32_t>( *aBlob, nksc::wb::kColorTempOffset ) == 5200 );
                CHECK( ReadAt<double>( *aBlob, nksc::wb::kBalanceOffset ) == 3.25 );

                // all other bytes unchanged
                std::vector<std::byte> aExpected = *aOriginalBlob;
                std::memcpy( aExpected.data() + nksc::wb::kColorTempOffset, aBlob->data() + nksc::wb::kColorTempOffset, 4 );
                std::memcpy( aExpected.data() + nksc::wb::kBalanceOffset, aBlob->data() + nksc::wb::kBalanceOffset, 8 );
                CHECK( *aBlob == aExpected );
            } );
    }
}

TEST( Edit_WhiteBalanceSingleValue )
{
    const fs::path kPath = NkscFiles().front();
    auto kFile = nksc::NkscFile::Load( kPath );
    REQUIRE( kFile );
    auto kEdits = nksc::NineEdits::Parse( kFile->NineEditsXml().value_or( "" ) );
    REQUIRE( kEdits );
    auto aBefore = WhiteBalanceBlob( *kEdits );
    REQUIRE( aBefore );

    REQUIRE( kEdits->SetWhiteBalance( std::nullopt, 1.5 ) );
    auto aAfter = WhiteBalanceBlob( *kEdits );
    REQUIRE( aAfter );
    CHECK( ReadAt<std::int32_t>( *aAfter, nksc::wb::kColorTempOffset ) == ReadAt<std::int32_t>( *aBefore, nksc::wb::kColorTempOffset ) );
    CHECK( ReadAt<double>( *aAfter, nksc::wb::kBalanceOffset ) == 1.5 );
}


// --- error paths --------------------------------------------------------------

TEST( Error_NoNineEdits )
{
    CHECK( !nksc::NkscFile::FromString( "<x:xmpmeta></x:xmpmeta>" ) );
    CHECK( !nksc::NkscFile::FromString( R"(<rdf:Description nine:NineEdits="&lt;userData/&gt;"/>)" ) );
    CHECK( !nksc::NkscFile::FromString( "<nine:NineEdits>&lt;userData/&gt;" ) );
    CHECK( !nksc::NkscFile::Load( s_kDataDir / "does-not-exist.nksc" ) );
}

TEST( Error_InvalidNineEdits )
{
    CHECK( !nksc::NineEdits::Parse( "<userData><filter></userData>" ) );
    CHECK( !nksc::NineEdits::Parse( "<other/>" ) );

    auto kEdits = nksc::NineEdits::Parse( "<userData/>" );
    REQUIRE( kEdits );
    CHECK( !kEdits->SetGain( "1" ) );
    CHECK( !kEdits->SetRoll( "1" ) );
    CHECK( !kEdits->SetCrop( {} ) );
    CHECK( !kEdits->SetWhiteBalance( 5000, std::nullopt ) );
    CHECK( !kEdits->SetGain( "abc" ) );
}

TEST( Error_UnsupportedWhiteBalanceVersion )
{
    auto kEdits = nksc::NineEdits::Parse(
        R"(<userData><filter id="nikon::WhiteBalance"><active>true</active><parameters>)"
        R"(<integer name="version">7</integer><binary name="Data">AAAA</binary></parameters></filter></userData>)" );
    REQUIRE( kEdits );
    const std::string sBefore = kEdits->Print();
    CHECK( !kEdits->SetWhiteBalance( 5000, std::nullopt ) );
    CHECK( kEdits->Print() == sBefore );
}


// --- main ---------------------------------------------------------------------

int main( int argc, char* argv[] )
{
    if( argc > 1 )
    {
        s_kDataDir = argv[1];
    }
    if( !fs::is_directory( s_kDataDir ) )
    {
        std::println( stderr, "test data directory not found: {}", s_kDataDir.string() );
        return 1;
    }

    for( const Test& kTest : Registry() )
    {
        s_sCurrentTest = kTest.sName;
        const int iFailuresBefore = s_iFailures;
        kTest.fnRun();
        std::println( "[{}] {}", s_iFailures == iFailuresBefore ? " OK " : "FAIL", kTest.sName );
    }

    std::println( "\n{} tests, {} checks, {} failures", Registry().size(), s_iChecks, s_iFailures );
    return s_iFailures == 0 ? 0 : 1;
}
