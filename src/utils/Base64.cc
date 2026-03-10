#include <nitrocoro/utils/Base64.h>

#include <stdexcept>

namespace nitrocoro::utils
{

// clang-format off
static constexpr char kEncTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static constexpr int8_t kDecTable[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};
// clang-format on

std::string base64Encode(std::string_view input)
{
    size_t outLen = ((input.size() + 2) / 3) * 4;
    std::string result(outLen, '\0');
    const auto * src = reinterpret_cast<const uint8_t *>(input.data());
    size_t i = 0, j = 0;
    for (; i + 2 < input.size(); i += 3)
    {
        result[j++] = kEncTable[src[i] >> 2];
        result[j++] = kEncTable[((src[i] & 3) << 4) | (src[i + 1] >> 4)];
        result[j++] = kEncTable[((src[i + 1] & 0xf) << 2) | (src[i + 2] >> 6)];
        result[j++] = kEncTable[src[i + 2] & 0x3f];
    }
    if (i < input.size())
    {
        result[j++] = kEncTable[src[i] >> 2];
        if (i + 1 < input.size())
        {
            result[j++] = kEncTable[((src[i] & 3) << 4) | (src[i + 1] >> 4)];
            result[j++] = kEncTable[(src[i + 1] & 0xf) << 2];
        }
        else
        {
            result[j++] = kEncTable[(src[i] & 3) << 4];
            result[j++] = '=';
        }
        result[j++] = '=';
    }
    return result;
}

std::string base64Decode(std::string_view input)
{
    std::string padded;
    if (input.size() % 4 != 0)
    {
        padded = std::string(input);
        padded.append(4 - input.size() % 4, '=');
        input = padded;
    }

    size_t outLen = (input.size() / 4) * 3;
    if (input[input.size() - 1] == '=')
        --outLen;
    if (input[input.size() - 2] == '=')
        --outLen;

    std::string result(outLen, '\0');
    size_t j = 0;
    for (size_t i = 0; i < input.size(); i += 4)
    {
        int8_t a = kDecTable[static_cast<uint8_t>(input[i])];
        int8_t b = kDecTable[static_cast<uint8_t>(input[i + 1])];
        int8_t c = kDecTable[static_cast<uint8_t>(input[i + 2])];
        int8_t d = kDecTable[static_cast<uint8_t>(input[i + 3])];
        if (a < 0 || b < 0)
            throw std::invalid_argument("base64Decode: invalid character");
        result[j++] = static_cast<char>((a << 2) | (b >> 4));

        if (input[i + 2] != '=')
        {
            if (c < 0)
                throw std::invalid_argument("base64Decode: invalid character");
            result[j++] = static_cast<char>(((b & 0xf) << 4) | (c >> 2));
        }

        if (input[i + 3] != '=')
        {
            if (d < 0)
                throw std::invalid_argument("base64Decode: invalid character");
            result[j++] = static_cast<char>(((c & 3) << 6) | d);
        }
    }
    return result;
}

} // namespace nitrocoro::utils
