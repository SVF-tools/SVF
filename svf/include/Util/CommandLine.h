#ifndef COMMANDLINE_H_
#define COMMANDLINE_H_

#include <algorithm>
#include <cassert>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <map>
#include <vector>
#include <cstdlib>
#include <iostream>
#include <sstream>

typedef unsigned u32_t;

class OptionBase
{
protected:
    /// Name/description pairs.
    typedef std::pair<std::string, std::string> PossibilityDescription;
    typedef std::vector<std::pair<std::string, std::string>> PossibilityDescriptions;

    /// Value/name/description tuples. If [1] is the value on the commandline for an option, we'd
    /// set the value for the associated Option to [0].
    template <typename T>
    using OptionPossibility = std::tuple<T, std::string, std::string>;

protected:
    OptionBase(std::string name, std::string description)
        : OptionBase(name, description, {})
    {
    }

    OptionBase(std::string name, std::string description, PossibilityDescriptions possibilityDescriptions)
        : name(name), description(description), possibilityDescriptions(possibilityDescriptions)
    {
        assert(name[0] != '-' && "OptionBase: name starts with '-'");
        assert(!isHelpName(name) && "OptionBase: reserved help name");
        assert(getOptionsMap().find(name) == getOptionsMap().end() && "OptionBase: duplicate option");

        // Types with empty names (i.e., OptionMultiple) can handle things themselves.
        if (!name.empty())
        {
            // Standard name=value option.
            getOptionsMap()[name] = this;
        }
    }

    /// From a given string, set the value of this option.
    virtual bool parseAndSetValue(const std::string value) = 0;

    /// Whether this option represents a boolean. Important as such
    /// arguments don't require a value.
    virtual bool isBool(void) const
    {
        return false;
    }

    /// Whether this option is an OptionMultiple.
    virtual bool isMultiple(void) const
    {
        return false;
    }

    /// Can this option be set?
    virtual bool canSet(void) const = 0;

public:
    /// Parse all constructed OptionBase children, returning positional arguments
    /// in the order they appeared.
    static std::vector<std::string> parseOptions(int argc, char *argv[], std::string description, std::string callFormat)
    {
        const std::string usage = buildUsage(description, std::string(argv[0]), callFormat);

        std::vector<std::string> positionalArguments;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg(argv[i]);
            if (arg.empty()) continue;
            if (arg[0] != '-')
            {
                // Positional argument. NOT a value to another argument because we
                // "skip" over evaluating those without the corresponding argument.
                positionalArguments.push_back(arg);
                continue;
            }

            // Chop off '-'.
            arg = arg.substr(1);

            std::string argName;
            std::string argValue;
            OptionBase *opt = nullptr;

            size_t equalsSign = arg.find('=');
            if (equalsSign != std::string::npos)
            {
                // Argument has an equal sign, i.e. argName=argValue
                argName = arg.substr(0, equalsSign);
                if (isHelpName(argName)) usageAndExit(usage, false);

                opt = getOption(argName);
                if (opt == nullptr)
                {
                    std::cerr << "Unknown option: " << argName << std::endl;
                    usageAndExit(usage, true);
                }

                argValue = arg.substr(equalsSign + 1);
            }
            else
            {
                argName = arg;
                if (isHelpName(argName)) usageAndExit(usage, false);

                opt = getOption(argName);
                if (opt == nullptr)
                {
                    std::cerr << "Unknown option: " << argName << std::endl;
                    usageAndExit(usage, true);
                }

                // No equals sign means we may need next argument.
                if (opt->isBool())
                {
                    // Booleans do not accept -arg true/-arg false.
                    // They must be -arg=true/-arg=false.
                    argValue = "true";
                }
                else if (opt->isMultiple())
                {
                    // Name is the value and will be converted to an enum.
                    argValue = argName;
                }
                else if (i + 1 < argc)
                {
                    // On iteration, we'll skip the value.
                    ++i;
                    argValue = std::string(argv[i]);
                }
                else
                {
                    std::cerr << "Expected value for: " << argName << std::endl;
                    usageAndExit(usage, true);
                }
            }

            if (!opt->canSet())
            {
                std::cerr << "Unable to set: " << argName << "; check for duplicates" << std::endl;
                usageAndExit(usage, true);
            }

            bool valueSet = opt->parseAndSetValue(argValue);
            if (!valueSet)
            {
                std::cerr << "Bad value for: " << argName << std::endl;
                usageAndExit(usage, true);
            }
        }

        return positionalArguments;
    }

private:
    /// Sets the usage member to a usage string, built from the static list of options.
    /// argv0 is argv[0] and callFormat is how the command should be used, minus the command
    /// name (e.g. "[options] <input-bitcode...>".
    static std::string buildUsage(
        const std::string description, const std::string argv0, const std::string callFormat
    )
    {
        // Determine longest option to split into two columns: options and descriptions.
        unsigned longest = 0;
        for (const std::pair<std::string, OptionBase *> nopt : getOptionsMap())
        {
            const std::string name = std::get<0>(nopt);
            const OptionBase *option = std::get<1>(nopt);
            if (option->isMultiple())
            {
                // For Multiple, description goes in left column.
                if (option->description.length() > longest) longest = option->description.length();
            }
            else
            {
                if (name.length() > longest) longest = name.length();
            }

            for (const PossibilityDescription &pd : option->possibilityDescriptions)
            {
                const std::string possibility = std::get<0>(pd);
                if (possibility.length() + 3 > longest) longest = possibility.length() + 3;
            }
        }

        std::stringstream ss;

        ss << description << std::endl << std::endl;

        ss << "USAGE:" << std::endl;
        ss << "  " << argv0 << " " << callFormat << std::endl;
        ss << std::endl;

        ss << "OPTIONS:" << std::endl;

        // Required as we have OptionMultiples doing a many-to-one in options.
        std::unordered_set<const OptionBase *> handled;
        for (const std::pair<std::string, OptionBase *> nopt : getOptionsMap())
        {
            const std::string name = std::get<0>(nopt);
            const OptionBase *option = std::get<1>(nopt);
            if (handled.find(option) != handled.end()) continue;
            handled.insert(option);

            if (option->isMultiple())
            {
                // description
                //   -name1      - description
                //   -name2      - description
                //   ...
                ss << "  " << option->description << std::endl;
                for (const PossibilityDescription &pd : option->possibilityDescriptions)
                {
                    const std::string possibility = std::get<0>(pd);
                    const std::string description = std::get<1>(pd);
                    ss << "    -" << possibility << std::string(longest - possibility.length() + 2, ' ');
                    ss << "- " << description << std::endl;
                }
            }
            else
            {
                // name  - description
                // or
                // name     - description
                //   =opt1    - description
                //   =opt2    - description
                //   ...
                ss << "  -" << name << std::string(longest - name.length() + 2, ' ');
                ss << "- " << option->description << std::endl;
                for (const PossibilityDescription &pd : option->possibilityDescriptions)
                {
                    const std::string possibility = std::get<0>(pd);
                    const std::string description = std::get<1>(pd);
                    ss << "    =" << possibility << std::string(longest - possibility.length() + 2, ' ');
                    ss << "- " << description << std::endl;
                }
            }
        }

        // Help message.
        ss << std::endl;
        ss << "  -help" << std::string(longest - 4 + 2, ' ') << "- show usage and exit" << std::endl;
        ss << "  -h" << std::string(longest - 1 + 2, ' ') << "- show usage and exit" << std::endl;

        // How to set boolean options.
        ss << std::endl;
        ss << "Note: for boolean options, -name true and -name false are invalid." << std::endl;
        ss << "      Use -name, -name=true, or -name=false." << std::endl;

        return ss.str();
    }

    /// Find option based on name in options map. Returns nullptr if not found.
    static OptionBase *getOption(const std::string optName)
    {
        auto optIt = getOptionsMap().find(optName);
        if (optIt == getOptionsMap().end()) return nullptr;
        else return optIt->second;
    }

    /// Print usage and exit. If error is set, print to stderr and exits with code 1.
    static void usageAndExit(const std::string usage, bool error)
    {
        if (error) std::cerr << usage;
        else std::cout << usage;
        std::exit(error ? 1 : 0);
    }

    /// Returns whether name is one of the reserved help options.
    static bool isHelpName(const std::string name)
    {
        static std::vector<std::string> helpNames = {"help", "h", "-help"};
        return std::find(helpNames.begin(), helpNames.end(), name) != helpNames.end();
    }

protected:
    // Return the name/description part of OptionsPossibilities (second and third fields).
    template<typename T>
    static PossibilityDescriptions extractPossibilityDescriptions(const std::vector<OptionPossibility<T>> possibilities)
    {
        PossibilityDescriptions possibilityDescriptions;
        for (const OptionPossibility<T> &op : possibilities)
        {
            possibilityDescriptions.push_back(std::make_pair(std::get<1>(op), std::get<2>(op)));
        }

        return possibilityDescriptions;
    }

    /// Not unordered map so we can have sorted names when building the usage string.
    /// Map of option names to their object.
    static std::map<std::string, OptionBase *> &getOptionsMap(void)
    {
        // Not static member to avoid initialisation order problems.
        static std::map<std::string, OptionBase *> options;
        return options;
    }


protected:
    std::string name;
    std::string description;
    /// For when we have possibilities like in an OptionMap.
    PossibilityDescriptions possibilityDescriptions;
};

/// General -name=value options.
/// Retrieve value by Opt().
template <typename T>
class Option : public OptionBase
{
public:
    Option(const std::string& name, const std::string& description, T init)
        : OptionBase(name, description), isExplicitlySet(false), value(init)
    {
        assert(!name.empty() && "Option: empty option name given");
    }

    virtual bool canSet(void) const override
    {
        // Don't allow duplicates.
        return !isExplicitlySet;
    }

    virtual bool parseAndSetValue(const std::string s) override
    {
        isExplicitlySet = fromString(s, value);
        return isExplicitlySet;
    }

    virtual bool isBool(void) const override
    {
        return std::is_same<T, bool>::value;
    }

    void setValue(T v)
    {
        value = v;
    }

    T operator()(void) const
    {
        return value;
    }

private:
    // Convert string to boolean, returning whether we succeeded.
    static bool fromString(const std::string& s, bool& value)
    {
        if (s == "true")
            value = true;
        else if (s == "false")
            value = false;
        else
            return false;
        return true;
    }

    // Convert string to string, always succeeds.
    static bool fromString(const std::string s, std::string &value)
    {
        value = s;
        return true;
    }

    // Convert string to u32_t, returning whether we succeeded.
    static bool fromString(const std::string s, u32_t &value)
    {
        // We won't allow anything except [0-9]+.
        if (s.empty()) return false;
        for (char c : s)
        {
            if (!(c >= '0' && c <= '9')) return false;
        }

        // Use strtoul because we're not using exceptions.
        assert(sizeof(unsigned long) >= sizeof(u32_t));
        const unsigned long sv = std::strtoul(s.c_str(), nullptr, 10);

        // Out of range according to strtoul, or according to us compared to u32_t.
        if (errno == ERANGE || sv > std::numeric_limits<u32_t>::max()) return false;
        value = sv;
        return true;
    }

private:
    bool isExplicitlySet;
    T value;
};

/// Option allowing a limited range of values, probably corresponding to an enum.
/// Opt() gives the value.
template <typename T>
class OptionMap : public OptionBase
{
public:
    typedef std::vector<OptionPossibility<T>> OptionPossibilities;

    OptionMap(std::string name, std::string description, T init, OptionPossibilities possibilities)
        : OptionBase(name, description, extractPossibilityDescriptions(possibilities)),
          isExplicitlySet(false), value(init), possibilities(possibilities)
    {
        assert(!name.empty() && "OptionMap: empty option name given");
    }

    virtual bool canSet(void) const override
    {
        return !isExplicitlySet;
    }

    virtual bool parseAndSetValue(const std::string s) override
    {
        for (const OptionPossibility<T> &op : possibilities)
        {
            // Check if the given string is a valid one.
            if (s == std::get<1>(op))
            {
                // What that string maps to.
                value = std::get<0>(op);
                isExplicitlySet = true;
                break;
            }
        }

        return isExplicitlySet;
    }

    T operator()(void) const
    {
        return value;
    }

private:
    bool isExplicitlySet;
    T value;
    OptionPossibilities possibilities;
};

/// Options which form a kind of bit set. There are multiple names and n of them can be set.
/// Opt(v) tests whether v had been set, aborting if v is invalid.
template <typename T>
class OptionMultiple : public OptionBase
{
public:
    typedef std::vector<OptionPossibility<T>> OptionPossibilities;

    OptionMultiple(std::string description, OptionPossibilities possibilities)
        : OptionBase("", description, extractPossibilityDescriptions(possibilities)), possibilities(possibilities)
    {
        for (const OptionPossibility<T> &op : possibilities)
        {
            optionValues[std::get<0>(op)] = false;
        }

        for (const OptionPossibility<T> &op : possibilities)
        {
            std::string possibilityName = std::get<1>(op);
            getOptionsMap()[possibilityName] = this;
        }
    }

    virtual bool canSet(void) const override
    {
        return true;
    }

    virtual bool parseAndSetValue(const std::string s) override
    {
        // Like in OptionMap basically, except we can have many values.
        for (const OptionPossibility<T> &op : possibilities)
        {
            if (s == std::get<1>(op))
            {
                optionValues[std::get<0>(op)] = true;
                return true;
            }
        }

        return false;
    }

    virtual bool isMultiple(void) const override
    {
        return true;
    }

    /// If the bitset is empty.
    bool nothingSet(void) const
    {
        for (const std::pair<T, bool> tb : optionValues)
        {
            if (tb.second) return false;
        }

        return true;
    }

    /// Returns whether the value v had been set as an option.
    bool operator()(const T v) const
    {
        typename std::unordered_map<T, bool>::const_iterator ovIt = optionValues.find(v);
        // TODO: disabled as we check for "invalid" values in some places.
        // assert(ovIt != optionValues.end() && "OptionMultiple: bad value checked");
        if (ovIt == optionValues.end()) return false;
        return ovIt->second;
    }

private:
    /// Is the option set? Use a map rather than a set because it's now easier in one
    /// DS to determine whether something is 1) valid, and 2) set.
    std::unordered_map<T, bool> optionValues;
    OptionPossibilities possibilities;
};

#endif  /* COMMANDLINE_H_ */
