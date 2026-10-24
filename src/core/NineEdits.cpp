#include "NineEdits.h"

#include "Base64.h"
#include "WhiteBalance.h"

#include <charconv>
#include <cstring>
#include <format>
#include <ranges>
#include <vector>


namespace nksc {

using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;


namespace {

// Prints like NX Studio: empty elements are self-closing, except <binary>, which
// NX Studio always writes with an explicit end tag.
class NxPrinter : public tinyxml2::XMLPrinter
{
public:

    using XMLPrinter::XMLPrinter;

    bool VisitEnter( const XMLElement& rElement, const tinyxml2::XMLAttribute* pAttribute ) override
    {
        const bool bResult = XMLPrinter::VisitEnter( rElement, pAttribute );
        if( rElement.NoChildren() && std::string_view( rElement.Name() ) == "binary" )
        {
            PushText( "" );
        }
        return bResult;
    }
};


template<typename T>
std::expected<T, std::string> ParseNumber( std::string_view sText, std::string_view sTypeName )
{
    T value{};
    const auto [pEnd, eError] = std::from_chars( sText.data(), sText.data() + sText.size(), value );
    if( sText.empty() || eError != std::errc{} || pEnd != sText.data() + sText.size() )
    {
        return std::unexpected( std::format( "'{}' is not a valid {}", sText, sTypeName ) );
    }
    return value;
}


// Returns the first child <sTag sAttr="sValue"> of pParent or nullptr.
XMLElement* FindChild( XMLElement* pParent, const char* sTag, const char* sAttr, std::string_view sValue )
{
    if( pParent == nullptr )
    {
        return nullptr;
    }
    for( XMLElement* pChild = pParent->FirstChildElement( sTag ); pChild != nullptr; pChild = pChild->NextSiblingElement( sTag ) )
    {
        const char* sAttrValue = pChild->Attribute( sAttr );
        if( sAttrValue != nullptr && sValue == sAttrValue )
        {
            return pChild;
        }
    }
    return nullptr;
}


XMLElement* AddParam( XMLElement* pParams, const char* sType, const char* sName, const char* sValue )
{
    XMLElement* pParam = pParams->InsertNewChildElement( sType );
    pParam->SetAttribute( "name", sName );
    if( sValue != nullptr )
    {
        pParam->SetText( sValue );
    }
    return pParam;
}


// Creates an inactive history step with the default parameters NX Studio writes for the filter.
XMLElement* NewHistoryStep( XMLDocument& rDoc, std::string_view sFilterId )
{
    XMLElement* pHistory = rDoc.NewElement( "historystep" );
    pHistory->InsertNewChildElement( "version" )->SetText( "20" );
    pHistory->InsertNewChildElement( "feather" )->SetText( "0" );
    pHistory->InsertNewChildElement( "featherEnabled" )->SetText( "false" );
    pHistory->InsertNewChildElement( "baseFill" )->SetText( "0" );
    pHistory->InsertNewChildElement( "baseFillEnabled" )->SetText( "true" );
    pHistory->InsertNewChildElement( "paintToolsEnabled" )->SetText( "true" );
    pHistory->InsertNewChildElement( "mixerEnabled" )->SetText( "true" );

    XMLElement* pAdjust = pHistory->InsertNewChildElement( "adjustmentData" )->InsertNewChildElement( "data" );
    pAdjust->SetAttribute( "id", "NkOneStepAdjustment" );
    pAdjust->SetText( "true" );

    XMLElement* pFilter = pHistory->InsertNewChildElement( "filter" );
    pFilter->SetAttribute( "id", std::string( sFilterId ).c_str() );
    pFilter->InsertNewChildElement( "active" )->SetText( "false" );

    XMLElement* pParams = pFilter->InsertNewChildElement( "parameters" );

    if( sFilterId == "nik::Newton" )
    {
        AddParam( pParams, "integer", "HandleCount", "0" );
    }
    else if( sFilterId == "nik::Straighten" )
    {
        AddParam( pParams, "integer", "autoCrop", "0" );
        AddParam( pParams, "double", "rotateAngle", "0" );
        AddParam( pParams, "integer", "rotateMode", "2" );
    }
    else if( sFilterId == "nik::Rotate" )
    {
        AddParam( pParams, "double", "rotateAngle", "0" );
        AddParam( pParams, "integer", "rotateMode", "2" );
    }
    else if( sFilterId == "nik::Crop" )
    {
        AddParam( pParams, "integer", "version", "1" );
        XMLElement* pPoint = AddParam( pParams, "point", "cropStart", nullptr );
        pPoint->SetAttribute( "x", -1 );
        pPoint->SetAttribute( "y", -1 );
        pPoint = AddParam( pParams, "point", "cropEnd", nullptr );
        pPoint->SetAttribute( "x", -1 );
        pPoint->SetAttribute( "y", -1 );
    }

    AddParam( pParams, "integer", "Version", "1" );
    AddParam( pParams, "integer", "nik.BlendingMode", "0" );
    AddParam( pParams, "integer", "nik.ChannelMode", "0" );
    AddParam( pParams, "integer", "nik.MixerActive", "0" );
    AddParam( pParams, "integer", "nik.Opacity1", "100" );
    AddParam( pParams, "integer", "nik.Opacity2", "100" );
    AddParam( pParams, "integer", "nik.Opacity3", "100" );

    return pHistory;
}


XMLElement* FindHistoryStep( XMLElement* pParams, std::string_view sFilterId )
{
    for( XMLElement* pStep = pParams->FirstChildElement( "historystep" ); pStep != nullptr; pStep = pStep->NextSiblingElement( "historystep" ) )
    {
        if( FindChild( pStep, "filter", "id", sFilterId ) != nullptr )
        {
            return pStep;
        }
    }
    return nullptr;
}

} // namespace


std::expected<int, std::string> ParseInt( std::string_view sText )
{
    return ParseNumber<int>( sText, "integer" );
}


std::expected<double, std::string> ParseDouble( std::string_view sText )
{
    return ParseNumber<double>( sText, "number" );
}


std::expected<CropRect, std::string> ParseCrop( std::string_view sText )
{
    std::vector<int> aValues;
    for( auto&& kPart : sText | std::views::split( ',' ) )
    {
        auto iValue = ParseInt( std::string_view( kPart ) );
        if( !iValue )
        {
            return std::unexpected( "crop: " + iValue.error() );
        }
        aValues.push_back( *iValue );
    }

    if( aValues.size() != 4 )
    {
        return std::unexpected( "crop expects 4 values separated by comma (x1,y1,x2,y2)" );
    }

    return CropRect{ aValues[0], aValues[1], aValues[2], aValues[3] };
}


std::expected<NineEdits, std::string> NineEdits::Parse( std::string_view sXml )
{
    NineEdits kEdits;
    kEdits.m_pDoc = std::make_unique<XMLDocument>();

    if( kEdits.m_pDoc->Parse( sXml.data(), sXml.size() ) != tinyxml2::XML_SUCCESS )
    {
        return std::unexpected( std::format( "NineEdits is not valid XML: {}", kEdits.m_pDoc->ErrorStr() ) );
    }
    if( kEdits.Root() == nullptr )
    {
        return std::unexpected( "NineEdits has no <userData> root element" );
    }

    return kEdits;
}


std::string NineEdits::Print() const
{
    NxPrinter kPrinter( nullptr, true, 0 );
    m_pDoc->Print( &kPrinter );
    return std::string( kPrinter.CStr(), kPrinter.CStrSize() - 1 );
}


std::string NineEdits::Dump() const
{
    NxPrinter kPrinter( nullptr, false, 0 );
    m_pDoc->Print( &kPrinter );

    std::string sResult;
    for( const char* p = kPrinter.CStr(); *p != '\0'; ++p )
    {
        if( *p == '\n' )
        {
            sResult += '\r';
        }
        sResult += *p;
    }
    return sResult;
}


XMLElement* NineEdits::FindFilter( std::string_view sId ) const
{
    return FindChild( Root(), "filter", "id", sId );
}


std::expected<void, std::string> NineEdits::SetGain( std::string_view sValue )
{
    if( auto dValue = ParseDouble( sValue ); !dValue )
    {
        return std::unexpected( "gain: " + dValue.error() );
    }

    XMLElement* pFilter = FindFilter( "nikon::ExposureSettings" );
    XMLElement* pParams = pFilter ? pFilter->FirstChildElement( "parameters" ) : nullptr;
    XMLElement* pGain = pParams ? pParams->FirstChildElement( "Gain" ) : nullptr;
    if( pGain == nullptr )
    {
        return std::unexpected( "nikon::ExposureSettings/Gain not found in nksc file" );
    }

    pGain->SetText( std::string( sValue ).c_str() );
    return {};
}


std::expected<void, std::string> NineEdits::SetWhiteBalance( std::optional<int> iColorTemp, std::optional<double> dBalance )
{
    // the blob first, so that nothing is changed if its format is unknown
    XMLElement* pFilter = FindFilter( "nikon::WhiteBalance" );
    XMLElement* pParams = pFilter ? pFilter->FirstChildElement( "parameters" ) : nullptr;
    XMLElement* pVersion = pParams ? pParams->FirstChildElement( "integer" ) : nullptr;
    XMLElement* pBinary = pParams ? pParams->FirstChildElement( "binary" ) : nullptr;
    if( pVersion == nullptr || pBinary == nullptr )
    {
        return std::unexpected( "nikon::WhiteBalance not found in nksc file" );
    }

    const int iVersion = pVersion->IntText( -1 );
    const std::size_t uExpectedSize = iVersion == 8 ? wb::kSizeVersion8
                                    : iVersion == 9 ? wb::kSizeVersion9
                                    : 0;
    if( uExpectedSize == 0 )
    {
        return std::unexpected( std::format( "unsupported nikon::WhiteBalance version: {}", iVersion ) );
    }

    auto aData = base64::Decode( pBinary->GetText() ? pBinary->GetText() : "" );
    if( !aData )
    {
        return std::unexpected( "nikon::WhiteBalance: " + aData.error() );
    }
    if( aData->size() != uExpectedSize )
    {
        return std::unexpected( std::format( "nikon::WhiteBalance version {} data has {} bytes, expected {}", iVersion, aData->size(), uExpectedSize ) );
    }

    // switch the white balance method, so that manually set values are accepted
    XMLElement* pAppFilter = FindFilter( "nikon::ApplicationData" );
    XMLElement* pAppParams = pAppFilter ? pAppFilter->FirstChildElement( "parameters" ) : nullptr;
    if( pAppParams == nullptr )
    {
        return std::unexpected( "nikon::ApplicationData not found in nksc file" );
    }

    XMLElement* pWbMethod = FindChild( pAppParams, "data", "id", "app:WB_method" );
    if( pWbMethod == nullptr )
    {
        pWbMethod = pAppParams->InsertNewChildElement( "data" );
        pWbMethod->SetAttribute( "id", "app:WB_method" );
    }
    pWbMethod->SetText( "NKD_WB_RECORDED_AND_EDIT" ); // NKD_WB_AUTO_AND_EDIT

    if( iColorTemp )
    {
        std::memcpy( aData->data() + wb::kColorTempOffset, &*iColorTemp, sizeof( std::int32_t ) );
    }
    if( dBalance )
    {
        std::memcpy( aData->data() + wb::kBalanceOffset, &*dBalance, sizeof( double ) );
    }

    pBinary->SetText( base64::Encode( *aData ).c_str() );
    return {};
}


// Makes sure the NXHistory contains all history steps in NX Studio order (adding missing
// ones, e.g. in files written by older versions of this tool), activates the requested
// filter and returns its parameters.
std::expected<XMLElement*, std::string> NineEdits::GetHistoryFilterParams( const char* sFilterId )
{
    XMLElement* pFilter = FindFilter( "nikon::NXHistory" );
    if( pFilter == nullptr )
    {
        return std::unexpected( "nikon::NXHistory not found in nksc file" );
    }

    XMLElement* pParams = pFilter->FirstChildElement( "parameters" );
    if( pParams == nullptr )
    {
        pParams = pFilter->InsertNewChildElement( "parameters" );
    }

    XMLElement* pPrevStep = nullptr;
    for( const char* sId : s_aHistoryFilterIds )
    {
        XMLElement* pStep = FindHistoryStep( pParams, sId );
        if( pStep == nullptr )
        {
            pStep = NewHistoryStep( *m_pDoc, sId );
            if( pPrevStep == nullptr )
            {
                pParams->InsertFirstChild( pStep );
            }
            else
            {
                pParams->InsertAfterChild( pPrevStep, pStep );
            }
        }
        pPrevStep = pStep;
    }

    XMLElement* pHistoryFilter = FindChild( FindHistoryStep( pParams, sFilterId ), "filter", "id", sFilterId );
    XMLElement* pActive = pHistoryFilter->FirstChildElement( "active" );
    XMLElement* pFilterParams = pHistoryFilter->FirstChildElement( "parameters" );
    if( pActive == nullptr || pFilterParams == nullptr )
    {
        return std::unexpected( std::format( "{} history step is incomplete in nksc file", sFilterId ) );
    }

    pActive->SetText( "true" );
    return pFilterParams;
}


std::expected<void, std::string> NineEdits::SetCrop( const CropRect& kRect )
{
    auto pParams = GetHistoryFilterParams( "nik::Crop" );
    if( !pParams )
    {
        return std::unexpected( pParams.error() );
    }

    XMLElement* pStart = FindChild( *pParams, "point", "name", "cropStart" );
    XMLElement* pEnd = FindChild( *pParams, "point", "name", "cropEnd" );
    if( pStart == nullptr || pEnd == nullptr )
    {
        return std::unexpected( "nik::Crop has no cropStart/cropEnd in nksc file" );
    }

    pStart->SetAttribute( "x", kRect.iX1 );
    pStart->SetAttribute( "y", kRect.iY1 );
    pEnd->SetAttribute( "x", kRect.iX2 );
    pEnd->SetAttribute( "y", kRect.iY2 );
    return {};
}


std::expected<void, std::string> NineEdits::SetRoll( std::string_view sDegrees )
{
    if( auto dValue = ParseDouble( sDegrees ); !dValue )
    {
        return std::unexpected( "roll: " + dValue.error() );
    }

    auto pParams = GetHistoryFilterParams( "nik::Straighten" );
    if( !pParams )
    {
        return std::unexpected( pParams.error() );
    }

    XMLElement* pAngle = FindChild( *pParams, "double", "name", "rotateAngle" );
    if( pAngle == nullptr )
    {
        return std::unexpected( "nik::Straighten has no rotateAngle in nksc file" );
    }

    pAngle->SetText( std::string( sDegrees ).c_str() );
    return {};
}

} // namespace nksc
