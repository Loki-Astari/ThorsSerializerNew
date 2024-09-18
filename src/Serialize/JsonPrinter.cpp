#include "JsonPrinter.h"
#include "JsonThor.h"
#include "ThorsIOUtil/Utility.h"
#include "ThorsLogging/ThorsLogging.h"
#include <iomanip>
#include <algorithm>

using namespace ThorsAnvil::Serialize;

namespace
{
    struct Prefix
    {

        static char const*  space[];
        static char const*  comma[];
        static char const*  colon[];

        OutputType                  characteristics;
        std::size_t                 size;
        PrintState&                 state;
        bool                        prefixValue;
        public:
            Prefix(OutputType characteristics, std::size_t size, PrintState& state, bool prefixValue = false)
                : characteristics(characteristics)
                , size(size - 1)
                , state(state)
                , prefixValue(prefixValue)
            {}
            void printSeporator(std::ostream& stream, bool key) const
            {
                char const*(&seporator)[] = (!key && std::get<1>(state) == TraitType::Map)
                                            ? colon
                                            : (std::get<0>(state) != 0) ? comma : space;
                stream << seporator[static_cast<int>(characteristics)];
            }
    };
    struct PrefixKey: public Prefix
    {
        using Prefix::Prefix;
        friend std::ostream& operator<<(std::ostream& stream, PrefixKey const& data)
        {
            data.printSeporator(stream, true);
            if (data.characteristics == OutputType::Stream) {
                return stream;
            }
            return stream << "\n" << std::string(data.size, '\t');
        }
    };
    struct PrefixValue: public Prefix
    {
        using Prefix::Prefix;
        friend std::ostream& operator<<(std::ostream& stream, PrefixValue const& data)
        {
            data.printSeporator(stream, false);
            ++std::get<0>(data.state);
            std::get<2>(data.state) = true;

            if (data.characteristics == OutputType::Stream) {
                return stream;
            }
            if (std::get<1>(data.state) == TraitType::Array && std::get<0>(data.state) != 1) {
                stream << " ";
            }
            if (std::get<1>(data.state) == TraitType::Map) {
                stream << " ";
            }
            return stream;
        }
    };
    struct PrefixMap: public Prefix
    {
        using Prefix::Prefix;
        friend std::ostream& operator<<(std::ostream& stream, PrefixMap const& data)
        {
            data.printSeporator(stream, false);
            if (data.characteristics == OutputType::Stream) {
                return stream;
            }
            return stream << "\n" << std::string(data.size, '\t');
        }
    };
    struct PrefixMapClose: public Prefix
    {
        using Prefix::Prefix;
        friend std::ostream& operator<<(std::ostream& stream, PrefixMapClose const& data)
        {
            ++std::get<0>(data.state);
            if (data.characteristics == OutputType::Stream) {
                return stream;
            }
            return stream << "\n" << std::string(data.size, '\t');
        }
    };
    struct PrefixArray: public Prefix
    {
        using Prefix::Prefix;
        friend std::ostream& operator<<(std::ostream& stream, PrefixArray const& data)
        {
            data.printSeporator(stream, false);
            if (data.characteristics == OutputType::Stream) {
                return stream;
            }
            return stream << "\n" << std::string(data.size, '\t');
        }
    };
    struct PrefixArrayClose: public Prefix
    {
        using Prefix::Prefix;
        friend std::ostream& operator<<(std::ostream& stream, PrefixArrayClose const& data)
        {
            ++std::get<0>(data.state);
            if (data.characteristics == OutputType::Stream) {
                return stream;
            }
            if (data.prefixValue) {
                return stream;
            }
            return stream << "\n" << std::string(data.size, '\t');
        }
    };
}

char const*  Prefix::space[]   = {"",  "",  ""};
char const*  Prefix::comma[]   = {",", ",", ","};
char const*  Prefix::colon[]   = {":", ":", ":"};


THORS_SERIALIZER_HEADER_ONLY_INCLUDE
JsonPrinter::JsonPrinter(std::ostream& output, PrinterConfig config)
    : PrinterInterface(output, config)
{
    state.emplace_back(0, TraitType::Value, false);
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::openDoc()
{}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::closeDoc()
{}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::openMap(std::size_t)
{
    output << PrefixMap(config.characteristics, state.size(), state.back()) << "{";
    state.emplace_back(0, TraitType::Map, false);
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::closeMap()
{
    if (std::get<1>(state.back()) != TraitType::Map)
    {
        ThorsLogAndThrow("ThorsAnvil::Serialize::JsonPrinter",
                         "closeMap",
                         "Invalid call to closeMap(): Currently not in a map");
    }
    bool prefixValue = std::get<2>(state.back());
    state.pop_back();
    output << PrefixMapClose(config.characteristics, state.size(), state.back(), prefixValue) << "}";
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::openArray(std::size_t)
{
    output << PrefixArray(config.characteristics, state.size(), state.back()) << "[";
    state.emplace_back(0, TraitType::Array, false);
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::closeArray()
{
    if (std::get<1>(state.back()) != TraitType::Array)
    {
        ThorsLogAndThrow("ThorsAnvil::Serialize::JsonPrinter",
                         "closeArray",
                         "Invalid call to closeArray(): Currently not in an array");
    }
    bool prefixValue = std::get<2>(state.back());
    state.pop_back();
    output << PrefixArrayClose(config.characteristics, state.size(), state.back(), prefixValue) << "]";
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::addKey(std::string const& key)
{
    if (std::get<1>(state.back()) != TraitType::Map)
    {
        ThorsLogAndThrow("ThorsAnvil::Serialize::JsonPrinter",
                         "addKey",
                         "Invalid call to addKey(): Currently not in a map");
    }
    output << PrefixKey(config.characteristics, state.size(), state.back()) << '"' << key << '"';
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonPrinter::addPrefix()
{
    output << PrefixValue(config.characteristics, state.size(), state.back());
}

template<typename T>
struct FormatDouble
{
    T const &  value;
    FormatDouble(T const& value): value(value){}
    friend std::ostream& operator<<(std::ostream& str, FormatDouble const& formater)
    {
        if (formater.value == 0)
        {
            return str << "0.0";
        }
        else
        {
            return str << formater.value;
        }
    }
};

THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(short int value)             {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(int value)                   {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(long int value)              {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(long long int value)         {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(unsigned short int value)    {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(unsigned int value)          {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(unsigned long int value)     {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(unsigned long long int value){output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(float value)                 {output << PrefixValue(config.characteristics, state.size(), state.back()) << FormatDouble<float>(value);}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(double value)                {output << PrefixValue(config.characteristics, state.size(), state.back()) << FormatDouble<double>(value);}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(long double value)           {output << PrefixValue(config.characteristics, state.size(), state.back()) << FormatDouble<long double>(value);}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(bool value)                  {output << PrefixValue(config.characteristics, state.size(), state.back()) << std::boolalpha << value;}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(std::string const& value)    {output << PrefixValue(config.characteristics, state.size(), state.back()) << '"' << EscapeString(value) << '"';}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addValue(std::string_view const&value){output << PrefixValue(config.characteristics, state.size(), state.back()) << '"' << EscapeString(value) << '"';}
THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addRawValue(std::string const& value) {output << PrefixValue(config.characteristics, state.size(), state.back()) << value;}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE void JsonPrinter::addNull()                             {output << PrefixValue(config.characteristics, state.size(), state.back()) << "null";}
