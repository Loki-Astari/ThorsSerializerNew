#include "JsonManualLexer.h"
#include "JsonLexemes.h"
#include "UnicodeIterator.h"
#include "ThorsIOUtil/Utility.h"
#include "ThorsLogging/ThorsLogging.h"

#include <limits>
#include <cstring>

using namespace ThorsAnvil::Serialize;

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
JsonManualLexer::JsonManualLexer(ParserInterface& parser)
    : parser(parser)
    , lastNull(false)
{}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
int JsonManualLexer::yylex()
{
    char    next    = parser.peekNextNonSpaceValue();
    //buffer.clear();
    lastNull = false;
    switch (next)
    {
        case '{':   lastToken = '{';return 1;
        case '}':   lastToken = '}';return 2;
        case '[':   lastToken = '[';return 3;
        case ']':   lastToken = ']';return 4;
        case ',':   lastToken = ',';return 5;
        case ':':   lastToken = ':';return 6;
        case 't':
        {
            lastBool = true;
            lastToken = ThorsAnvil::Serialize::JSON_TRUE;
            return 7;
        }
        case 'f':
        {
            lastBool = false;
            lastToken = ThorsAnvil::Serialize::JSON_FALSE;
            return 8;
        }
        case 'n':
        {
            lastNull = true;
            lastToken = ThorsAnvil::Serialize::JSON_NULL;
            return 9;
        }
        case '"':
        {
            lastToken = ThorsAnvil::Serialize::JSON_STRING;
            return 10;
        }
        default:
        {
            lastToken = ThorsAnvil::Serialize::JSON_NUMBER;
            return 13;
        }
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::readTrue()
{
    checkFixed("true", 4);
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::readFalse()
{
    checkFixed("false", 5);
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::readNull()
{
    checkFixed("null", 4);
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::ignoreRawValue()
{
    switch (lastToken)
    {
        case ThorsAnvil::Serialize::JSON_TRUE:      parser.ignore(4);break;
        case ThorsAnvil::Serialize::JSON_FALSE:     parser.ignore(5);break;
        case ThorsAnvil::Serialize::JSON_NULL:      parser.ignore(4);break;
        case ThorsAnvil::Serialize::JSON_NUMBER:    readNumber();break;
        case ThorsAnvil::Serialize::JSON_STRING:
        {
            char last = parser.get();  // Read the first Quote off the stream
            int next = parser.get();
            while (next != EOF && !(next == '"' && last != '\\'))
            {
                last = next;
                next = parser.get();
            }
            if (next == EOF)
            {
                error();
            }
            break;
        }
        default:break;
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
std::string_view JsonManualLexer::getRawString()
{
    switch (lastToken)
    {
        case '{':   return "{";
        case '}':   return "}";
        case '[':   return "[";
        case ']':   return "]";
        case ',':   return ",";
        case ':':   return ":";
        case ThorsAnvil::Serialize::JSON_TRUE:  parser.ignore(4);return "true";
        case ThorsAnvil::Serialize::JSON_FALSE: parser.ignore(5);return "false";
        case ThorsAnvil::Serialize::JSON_NULL:  parser.ignore(4);return "null";
        //case ThorsAnvil::Serialize::JSON_INTEGER:
        //case ThorsAnvil::Serialize::JSON_FLOAT:
        case ThorsAnvil::Serialize::JSON_NUMBER:
        {
            readNumber();
            return buffer;
        }
        case ThorsAnvil::Serialize::JSON_STRING:
        {
            buffer.clear();
            char last = parser.get();  // Read the first Quote off the stream
            buffer.push_back(last);
            int next = parser.get();
            while (next != EOF && !(next == '"' && last != '\\'))
            {
                if (next < 0x20)
                {
                    ThorsLogAndThrow("ThorsAnvil::Serializer::JsonManualLexer",
                                     "getRawString",
                                     "Strings should not contain control characters.");
                }
                buffer.push_back(next);
                last = next;
                next = parser.get();
            }
            if (next == EOF)
            {
                error();
            }
            buffer.push_back('"');
            return buffer;
        }
        default:
        {
            ThorsLogAndThrow("ThorsAnvil::Serializer::JsonManualLexer",
                             "getRawString",
                             "Don't know how to retrieve the RAW data from the JSON input");
        }
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
bool JsonManualLexer::checkEscape(std::string& reply)
{
    bool isEscape = false;
    for (std::size_t loop = reply.size(); (loop > 0) && (reply[loop - 1] == '\\'); --loop)
    {
        isEscape = !isEscape;
    }
    return isEscape;
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::getStringInto(std::string& value)
{
    //return std::string(make_UnicodeWrapperIterator(std::istreambuf_iterator<char>(str)), make_EndUnicodeWrapperIterator(std::istreambuf_iterator<char>(str)));
    char next = parser.get();
    if (next != '"')
    {
        ThorsLogAndThrow("ThorsAnvil::Serialize::UnicodeWrapperIterator",
                         "UnicodeWrapperIterator",
                         "String does not start with a \" character");
    }

    parser.readTo(value, '"');

    if (value.size() > 0 && checkEscape(value))
    {
        std::string tmp;
        do
        {
            value.append("\"");
            parser.readTo(tmp, '"');
            value.append(tmp);
        }
        while (tmp.size() > 0 && checkEscape(tmp));
    }
    if (parser.config.convertBackSlash)
    {
        auto newEnd = std::copy(make_UnicodeWrapperIterator(std::begin(value)),
                                make_EndUnicodeWrapperIterator(std::end(value)),
                                std::begin(value));
        value.resize(std::distance(std::begin(value), newEnd));
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
std::string_view JsonManualLexer::getString()
{
    getStringInto(buffer);
    return buffer;
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
bool JsonManualLexer::getLastBool()
{
    switch (lastToken)
    {
        case ThorsAnvil::Serialize::JSON_TRUE:
            readTrue();
            return lastBool;
        case ThorsAnvil::Serialize::JSON_FALSE:
            readFalse();
            return lastBool;
        default:
        {
            ThorsLogAndThrow("ThorsAnvil::Serialize::JsonParser",
                             "getLastBool",
                             "The last value was not a bool");
        }
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
bool JsonManualLexer::isLastNull()
{
    return lastNull;
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
char JsonManualLexer::readDigits(char next)
{
    if (!std::isdigit(next))
    {
        error();
    }
    while (std::isdigit(next))
    {
        buffer.push_back(next);
        next = parser.get();
    }
    return next;
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::readNumber()
{
    buffer.clear();

    int next = parser.get();

    if (next == '-' || next == '+')
    {
        buffer.push_back(next);
        next = parser.get();
        if (next == EOF)
        {   error();
        }
    }
    if (next == '0')
    {
        buffer.push_back(next);
        next = parser.get();
    }
    else
    {
        next = readDigits(next);
    }
    if (next == '.')
    {
        buffer.push_back(next);
        next        = parser.get();
        if (next == EOF)
        {   error();
        }
        next        = readDigits(next);
    }
    if (next == 'e' || next == 'E')
    {
        buffer.push_back(next);
        next = parser.get();
        if (next == EOF)
        {   error();
        }
        if (next == '-' || next == '+')
        {
            buffer.push_back(next);
            next = parser.get();
            if (next == EOF)
            {   error();
            }
        }
        next = readDigits(next);
    }
    if (next != EOF)
    {
        parser.unget();
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::checkFixed(char const* check, std::size_t size)
{
    buffer.resize(size);
    parser.read(&buffer[0], size);
    if (std::strncmp(&buffer[0], check, size) != 0)
    {
        error();
    }
}

THORS_SERIALIZER_HEADER_ONLY_INCLUDE
void JsonManualLexer::error()
{
    ThorsLogAndThrow("ThorsAnvil::Serialize::JsonManualLexer",
                     "error",
                     "Invalid Character in Lexer");
}
