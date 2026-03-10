#include <nitrocoro/utils/Md5.h>

#include <bit>
#include <cstring>

namespace nitrocoro::utils
{

// clang-format off
static constexpr uint32_t kS[64] = {
    7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
    5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
    4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
    6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21,
};

static constexpr uint32_t kK[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391,
};
// clang-format on

std::array<uint8_t, 16> md5(std::string_view input)
{
    uint64_t bitLen = static_cast<uint64_t>(input.size()) * 8;
    size_t padLen = input.size() + 1 + 8;
    padLen = (padLen + 63) & ~63u;

    std::string msg(padLen, '\0');
    std::memcpy(msg.data(), input.data(), input.size());
    msg[input.size()] = static_cast<char>(0x80);
    for (int i = 0; i < 8; ++i)
        msg[padLen - 8 + i] = static_cast<char>(bitLen >> (i * 8));

    uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;

    for (size_t offset = 0; offset < padLen; offset += 64)
    {
        uint32_t M[16];
        std::memcpy(M, msg.data() + offset, 64);

        uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; ++i)
        {
            uint32_t F;
            int g;
            if (i < 16)
            {
                F = (B & C) | (~B & D);
                g = i;
            }
            else if (i < 32)
            {
                F = (D & B) | (~D & C);
                g = (5 * i + 1) % 16;
            }
            else if (i < 48)
            {
                F = B ^ C ^ D;
                g = (3 * i + 5) % 16;
            }
            else
            {
                F = C ^ (B | ~D);
                g = (7 * i) % 16;
            }
            F += A + kK[i] + M[g];
            A = D;
            D = C;
            C = B;
            B += std::rotl(F, static_cast<int>(kS[i]));
        }
        a0 += A;
        b0 += B;
        c0 += C;
        d0 += D;
    }

    std::array<uint8_t, 16> digest{};
    auto store = [&](int i, uint32_t h) {
        digest[i * 4 + 0] = static_cast<uint8_t>(h);
        digest[i * 4 + 1] = static_cast<uint8_t>(h >> 8);
        digest[i * 4 + 2] = static_cast<uint8_t>(h >> 16);
        digest[i * 4 + 3] = static_cast<uint8_t>(h >> 24);
    };
    store(0, a0);
    store(1, b0);
    store(2, c0);
    store(3, d0);
    return digest;
}

std::string md5Hex(std::string_view input)
{
    auto d = md5(input);
    std::string result(32, '\0');
    static constexpr char hex[] = "0123456789abcdef";
    for (int i = 0; i < 16; ++i)
    {
        result[i * 2] = hex[d[i] >> 4];
        result[i * 2 + 1] = hex[d[i] & 0xf];
    }
    return result;
}

} // namespace nitrocoro::utils
