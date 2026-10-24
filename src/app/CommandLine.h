#pragma once

#include <deque>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>


namespace nksc {

// Minimal command line parser.
// Accepts "--name value", "--name=value" and "-n value" for options and "--name" / "-n" for flags.
// Everything not starting with '-' is a positional argument, "--" ends option parsing.
class CommandLine
{
public:

    // The returned references stay valid for the lifetime of the CommandLine.
    const bool& AddFlag( std::string_view sLongName, char cShortName, std::string_view sHelp );
    const std::optional<std::string>& AddOption( std::string_view sLongName, char cShortName, std::string_view sValueName, std::string_view sHelp );

    // Parses the arguments without the program name (argv + 1).
    std::expected<void, std::string> Parse( std::span<const char* const> aArgs );

    const std::vector<std::string>& Positionals() const { return m_aPositionals; }

    std::string Usage( std::string_view sProgramName, std::string_view sPositionalUsage ) const;

private:

    struct Argument
    {
        std::string sLongName;
        char        cShortName = 0;
        std::string sValueName;             // empty for flags
        std::string sHelp;
        bool        bSet = false;           // value of a flag
        std::optional<std::string> sValue;  // value of an option
    };

    Argument* Find( std::string_view sLongName, char cShortName );

    std::deque<Argument> m_aArguments; // deque: stable references
    std::vector<std::string> m_aPositionals;
};

} // namespace nksc
