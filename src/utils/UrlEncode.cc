#include <nitrocoro/utils/UrlEncode.h>

namespace nitrocoro::utils
{

static constexpr char kHex[] = "0123456789ABCDEF";

static bool isUnreserved(char c)
{
    return (c >= 'A' && c <= 'Z')
           || (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9')
           || c == '-' || c == '_' || c == '.' || c == '~';
}

static int hexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

static void percentEncode(std::string & result, char c)
{
    auto uc = static_cast<unsigned char>(c);
    result += '%';
    result += kHex[uc >> 4];
    result += kHex[uc & 0xf];
}

static std::string decodeImpl(std::string_view input, bool decodePlus, bool skipSlash)
{
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        char c = input[i];
        if (decodePlus && c == '+')
        {
            result += ' ';
        }
        else if (c == '%')
        {
            if (i + 2 < input.size())
            {
                int hi = hexVal(input[i + 1]);
                int lo = hexVal(input[i + 2]);
                if (hi >= 0 && lo >= 0)
                {
                    char decoded = static_cast<char>((hi << 4) | lo);
                    if (skipSlash && decoded == '/')
                        result += "%2F";
                    else
                        result += decoded;
                    i += 2;
                    continue;
                }
            }
            result += '%'; // invalid sequence, keep as-is
        }
        else
        {
            result += c;
        }
    }
    return result;
}

std::string urlDecode(std::string_view input)
{
    return decodeImpl(input, false, true);
}

std::string urlDecodeComponent(std::string_view input)
{
    return decodeImpl(input, true, false);
}

std::string urlEncode(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input)
    {
        if (isUnreserved(c) || c == '/')
            result += c;
        else if (c == '+')
            result += "%2B";
        else
            percentEncode(result, c);
    }
    return result;
}

std::string urlEncodeComponent(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input)
    {
        if (isUnreserved(c))
            result += c;
        else if (c == '+')
            result += "%2B";
        else
            percentEncode(result, c);
    }
    return result;
}

std::string formEncode(std::string_view input)
{
    std::string result;
    result.reserve(input.size());
    for (char c : input)
    {
        if (isUnreserved(c))
            result += c;
        else if (c == ' ')
            result += '+';
        else if (c == '+')
            result += "%2B";
        else
            percentEncode(result, c);
    }
    return result;
}

} // namespace nitrocoro::utils
