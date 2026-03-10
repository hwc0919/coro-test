#include <nitrocoro/utils/Sha1.h>

#include <bit>
#include <cstring>

namespace nitrocoro::utils
{

std::array<uint8_t, 20> sha1(std::string_view input)
{
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

    uint64_t bitLen = static_cast<uint64_t>(input.size()) * 8;
    size_t padLen = input.size() + 1 + 8;
    padLen = (padLen + 63) & ~63u;

    std::string msg(padLen, '\0');
    std::memcpy(msg.data(), input.data(), input.size());
    msg[input.size()] = static_cast<char>(0x80);
    for (int i = 0; i < 8; ++i)
        msg[padLen - 8 + i] = static_cast<char>(bitLen >> (56 - i * 8));

    for (size_t offset = 0; offset < padLen; offset += 64)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
        {
            const uint8_t * b = reinterpret_cast<const uint8_t *>(msg.data()) + offset + i * 4;
            w[i] = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) |
                   (static_cast<uint32_t>(b[2]) << 8) | b[3];
        }
        for (int i = 16; i < 80; ++i)
            w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i)
        {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d); k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;           k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;           k = 0xCA62C1D6; }
            uint32_t tmp = std::rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = std::rotl(b, 30); b = a; a = tmp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> digest{};
    auto store = [&](int i, uint32_t h) {
        digest[i * 4 + 0] = static_cast<uint8_t>(h >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(h >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(h >> 8);
        digest[i * 4 + 3] = static_cast<uint8_t>(h);
    };
    store(0, h0); store(1, h1); store(2, h2); store(3, h3); store(4, h4);
    return digest;
}

std::string sha1Hex(std::string_view input)
{
    auto d = sha1(input);
    std::string result(40, '\0');
    static constexpr char hex[] = "0123456789abcdef";
    for (int i = 0; i < 20; ++i)
    {
        result[i * 2]     = hex[d[i] >> 4];
        result[i * 2 + 1] = hex[d[i] & 0xf];
    }
    return result;
}

} // namespace nitrocoro::utils
