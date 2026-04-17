/**
 * @file Hpack.cc
 * @brief HPACK encoder/decoder implementation (RFC 7541)
 */
#include "Hpack.h"

#include <nitrocoro/http/HttpHeader.h>
#include <nitrocoro/http/HttpTypes.h>

#include <algorithm>
#include <nitrocoro/utils/Debug.h>
#include <stdexcept>

namespace nitrocoro::http2::hpack
{

// ── Huffman decode table (RFC 7541 Appendix B) ────────────────────────────────
// Each entry: { code, code_len, symbol }
// We use a simple canonical Huffman decoder via a lookup approach.

struct HuffSymbol
{
    uint32_t code;
    uint8_t bits;
};

// RFC 7541 Appendix B - 257 symbols (0-255 + EOS=256)
static const HuffSymbol kHuffTable[257] = {
    { 0x1ff8, 13 },     // 0
    { 0x7fffd8, 23 },   // 1
    { 0xfffffe2, 28 },  // 2
    { 0xfffffe3, 28 },  // 3
    { 0xfffffe4, 28 },  // 4
    { 0xfffffe5, 28 },  // 5
    { 0xfffffe6, 28 },  // 6
    { 0xfffffe7, 28 },  // 7
    { 0xfffffe8, 28 },  // 8
    { 0xffffea, 24 },   // 9
    { 0x3ffffffc, 30 }, // 10
    { 0xfffffe9, 28 },  // 11
    { 0xfffffea, 28 },  // 12
    { 0x3ffffffd, 30 }, // 13
    { 0xfffffeb, 28 },  // 14
    { 0xfffffec, 28 },  // 15
    { 0xfffffed, 28 },  // 16
    { 0xfffffee, 28 },  // 17
    { 0xfffffef, 28 },  // 18
    { 0xffffff0, 28 },  // 19
    { 0xffffff1, 28 },  // 20
    { 0xffffff2, 28 },  // 21
    { 0x3ffffffe, 30 }, // 22
    { 0xffffff3, 28 },  // 23
    { 0xffffff4, 28 },  // 24
    { 0xffffff5, 28 },  // 25
    { 0xffffff6, 28 },  // 26
    { 0xffffff7, 28 },  // 27
    { 0xffffff8, 28 },  // 28
    { 0xffffff9, 28 },  // 29
    { 0xffffffa, 28 },  // 30
    { 0xffffffb, 28 },  // 31
    { 0x14, 6 },        // 32  ' '
    { 0x3f8, 10 },      // 33  '!'
    { 0x3f9, 10 },      // 34  '"'
    { 0xffa, 12 },      // 35  '#'
    { 0x1ff9, 13 },     // 36  '$'
    { 0x15, 6 },        // 37  '%'
    { 0xf8, 8 },        // 38  '&'
    { 0x7fa, 11 },      // 39  '\''
    { 0x3fa, 10 },      // 40  '('
    { 0x3fb, 10 },      // 41  ')'
    { 0xf9, 8 },        // 42  '*'
    { 0x7fb, 11 },      // 43  '+'
    { 0xfa, 8 },        // 44  ','
    { 0x16, 6 },        // 45  '-'
    { 0x17, 6 },        // 46  '.'
    { 0x18, 6 },        // 47  '/'
    { 0x0, 5 },         // 48  '0'
    { 0x1, 5 },         // 49  '1'
    { 0x2, 5 },         // 50  '2'
    { 0x19, 6 },        // 51  '3'
    { 0x1a, 6 },        // 52  '4'
    { 0x1b, 6 },        // 53  '5'
    { 0x1c, 6 },        // 54  '6'
    { 0x1d, 6 },        // 55  '7'
    { 0x1e, 6 },        // 56  '8'
    { 0x1f, 6 },        // 57  '9'
    { 0x5c, 7 },        // 58  ':'
    { 0xfb, 8 },        // 59  ';'
    { 0x7ffc, 15 },     // 60  '<'
    { 0x20, 6 },        // 61  '='
    { 0xffb, 12 },      // 62  '>'
    { 0x3fc, 10 },      // 63  '?'
    { 0x1ffa, 13 },     // 64  '@'
    { 0x21, 6 },        // 65  'A'
    { 0x5d, 7 },        // 66  'B'
    { 0x5e, 7 },        // 67  'C'
    { 0x5f, 7 },        // 68  'D'
    { 0x60, 7 },        // 69  'E'
    { 0x61, 7 },        // 70  'F'
    { 0x62, 7 },        // 71  'G'
    { 0x63, 7 },        // 72  'H'
    { 0x64, 7 },        // 73  'I'
    { 0x65, 7 },        // 74  'J'
    { 0x66, 7 },        // 75  'K'
    { 0x67, 7 },        // 76  'L'
    { 0x68, 7 },        // 77  'M'
    { 0x69, 7 },        // 78  'N'
    { 0x6a, 7 },        // 79  'O'
    { 0x6b, 7 },        // 80  'P'
    { 0x6c, 7 },        // 81  'Q'
    { 0x6d, 7 },        // 82  'R'
    { 0x6e, 7 },        // 83  'S'
    { 0x6f, 7 },        // 84  'T'
    { 0x70, 7 },        // 85  'U'
    { 0x71, 7 },        // 86  'V'
    { 0x72, 7 },        // 87  'W'
    { 0xfc, 8 },        // 88  'X'
    { 0x73, 7 },        // 89  'Y'
    { 0xfd, 8 },        // 90  'Z'
    { 0x1ffb, 13 },     // 91  '['
    { 0x7fff0, 19 },    // 92  '\'
    { 0x1ffc, 13 },     // 93  ']'
    { 0x3ffc, 14 },     // 94  '^'
    { 0x22, 6 },        // 95  '_'
    { 0x7ffd, 15 },     // 96  '`'
    { 0x3, 5 },         // 97  'a'
    { 0x23, 6 },        // 98  'b'
    { 0x4, 5 },         // 99  'c'
    { 0x24, 6 },        // 100 'd'
    { 0x5, 5 },         // 101 'e'
    { 0x25, 6 },        // 102 'f'
    { 0x26, 6 },        // 103 'g'
    { 0x27, 6 },        // 104 'h'
    { 0x6, 5 },         // 105 'i'
    { 0x74, 7 },        // 106 'j'
    { 0x75, 7 },        // 107 'k'
    { 0x28, 6 },        // 108 'l'
    { 0x29, 6 },        // 109 'm'
    { 0x2a, 6 },        // 110 'n'
    { 0x7, 5 },         // 111 'o'
    { 0x2b, 6 },        // 112 'p'
    { 0x76, 7 },        // 113 'q'
    { 0x2c, 6 },        // 114 'r'
    { 0x8, 5 },         // 115 's'
    { 0x9, 5 },         // 116 't'
    { 0x2d, 6 },        // 117 'u'
    { 0x77, 7 },        // 118 'v'
    { 0x78, 7 },        // 119 'w'
    { 0x79, 7 },        // 120 'x'
    { 0x7a, 7 },        // 121 'y'
    { 0x7b, 7 },        // 122 'z'
    { 0x7ffe, 15 },     // 123 '{'
    { 0x7fc, 11 },      // 124 '|'
    { 0x3ffd, 14 },     // 125 '}'
    { 0x1ffd, 13 },     // 126 '~'
    { 0xffffffc, 28 },  // 127
    { 0xfffe6, 20 },    // 128
    { 0x3fffd2, 22 },   // 129
    { 0xfffe7, 20 },    // 130
    { 0xfffe8, 20 },    // 131
    { 0x3fffd3, 22 },   // 132
    { 0x3fffd4, 22 },   // 133
    { 0x3fffd5, 22 },   // 134
    { 0x7fffd9, 23 },   // 135
    { 0x3fffd6, 22 },   // 136
    { 0x7fffda, 23 },   // 137
    { 0x7fffdb, 23 },   // 138
    { 0x7fffdc, 23 },   // 139
    { 0x7fffdd, 23 },   // 140
    { 0x7fffde, 23 },   // 141
    { 0xffffeb, 24 },   // 142
    { 0x7fffdf, 23 },   // 143
    { 0xffffec, 24 },   // 144
    { 0xffffed, 24 },   // 145
    { 0x3fffd7, 22 },   // 146
    { 0x7fffe0, 23 },   // 147
    { 0xffffee, 24 },   // 148
    { 0x7fffe1, 23 },   // 149
    { 0x7fffe2, 23 },   // 150
    { 0x7fffe3, 23 },   // 151
    { 0x7fffe4, 23 },   // 152
    { 0x1fffdc, 21 },   // 153
    { 0x3fffd8, 22 },   // 154
    { 0x7fffe5, 23 },   // 155
    { 0x3fffd9, 22 },   // 156
    { 0x7fffe6, 23 },   // 157
    { 0x7fffe7, 23 },   // 158
    { 0xffffef, 24 },   // 159
    { 0x3fffda, 22 },   // 160
    { 0x1fffdd, 21 },   // 161
    { 0xfffe9, 20 },    // 162
    { 0x3fffdb, 22 },   // 163
    { 0x3fffdc, 22 },   // 164
    { 0x7fffe8, 23 },   // 165
    { 0x7fffe9, 23 },   // 166
    { 0x1fffde, 21 },   // 167
    { 0x7fffea, 23 },   // 168
    { 0x3fffdd, 22 },   // 169
    { 0x3fffde, 22 },   // 170
    { 0xfffff0, 24 },   // 171
    { 0x1fffdf, 21 },   // 172
    { 0x3fffdf, 22 },   // 173
    { 0x7fffeb, 23 },   // 174
    { 0x7fffec, 23 },   // 175
    { 0x1fffe0, 21 },   // 176
    { 0x1fffe1, 21 },   // 177
    { 0x3fffe0, 22 },   // 178
    { 0x1fffe2, 21 },   // 179
    { 0x7fffed, 23 },   // 180
    { 0x3fffe1, 22 },   // 181
    { 0x7fffee, 23 },   // 182
    { 0x7fffef, 23 },   // 183
    { 0xfffea, 20 },    // 184
    { 0x3fffe2, 22 },   // 185
    { 0x3fffe3, 22 },   // 186
    { 0x3fffe4, 22 },   // 187
    { 0x7ffff0, 23 },   // 188
    { 0x3fffe5, 22 },   // 189
    { 0x3fffe6, 22 },   // 190
    { 0x7ffff1, 23 },   // 191
    { 0x3ffffe0, 26 },  // 192
    { 0x3ffffe1, 26 },  // 193
    { 0xfffeb, 20 },    // 194
    { 0x7fff1, 19 },    // 195
    { 0x3fffe7, 22 },   // 196
    { 0x7ffff2, 23 },   // 197
    { 0x3fffe8, 22 },   // 198
    { 0x1ffffec, 25 },  // 199
    { 0x3ffffe2, 26 },  // 200
    { 0x3ffffe3, 26 },  // 201
    { 0x3ffffe4, 26 },  // 202
    { 0x7ffffde, 27 },  // 203
    { 0x7ffffdf, 27 },  // 204
    { 0x3ffffe5, 26 },  // 205
    { 0xfffff1, 24 },   // 206
    { 0x1ffffed, 25 },  // 207
    { 0x7fff2, 19 },    // 208
    { 0x1fffe3, 21 },   // 209
    { 0x3ffffe6, 26 },  // 210
    { 0x7ffffe0, 27 },  // 211
    { 0x7ffffe1, 27 },  // 212
    { 0x3ffffe7, 26 },  // 213
    { 0x7ffffe2, 27 },  // 214
    { 0xfffff2, 24 },   // 215
    { 0x1fffe4, 21 },   // 216
    { 0x1fffe5, 21 },   // 217
    { 0x3ffffe8, 26 },  // 218
    { 0x3ffffe9, 26 },  // 219
    { 0xffffffd, 28 },  // 220
    { 0x7ffffe3, 27 },  // 221
    { 0x7ffffe4, 27 },  // 222
    { 0x7ffffe5, 27 },  // 223
    { 0xfffec, 20 },    // 224
    { 0xfffff3, 24 },   // 225
    { 0xfffed, 20 },    // 226
    { 0x1fffe6, 21 },   // 227
    { 0x3fffe9, 22 },   // 228
    { 0x1fffe7, 21 },   // 229
    { 0x1fffe8, 21 },   // 230
    { 0x7ffff3, 23 },   // 231
    { 0x3fffea, 22 },   // 232
    { 0x3fffeb, 22 },   // 233
    { 0x1ffffee, 25 },  // 234
    { 0x1ffffef, 25 },  // 235
    { 0xfffff4, 24 },   // 236
    { 0xfffff5, 24 },   // 237
    { 0x3ffffea, 26 },  // 238
    { 0x7ffff4, 23 },   // 239
    { 0x3ffffeb, 26 },  // 240
    { 0x7ffffe6, 27 },  // 241
    { 0x3ffffec, 26 },  // 242
    { 0x3ffffed, 26 },  // 243
    { 0x7ffffe7, 27 },  // 244
    { 0x7ffffe8, 27 },  // 245
    { 0x7ffffe9, 27 },  // 246
    { 0x7ffffea, 27 },  // 247
    { 0x7ffffeb, 27 },  // 248
    { 0xffffffe, 28 },  // 249
    { 0x7ffffec, 27 },  // 250
    { 0x7ffffed, 27 },  // 251
    { 0x7ffffee, 27 },  // 252
    { 0x7ffffef, 27 },  // 253
    { 0x7fffff0, 27 },  // 254
    { 0x3ffffee, 26 },  // 255
    { 0x3fffffff, 30 }, // 256 EOS
};

static std::string huffmanDecode(const uint8_t * data, size_t len)
{
    // Build a simple decode: accumulate bits, match against table
    uint64_t bits = 0;
    int bitsLeft = 0;
    std::string result;
    result.reserve(len);

    size_t pos = 0;
    while (pos < len || bitsLeft > 0)
    {
        // Fill up to 32 bits
        while (bitsLeft < 32 && pos < len)
        {
            bits = (bits << 8) | data[pos++];
            bitsLeft += 8;
        }

        // Try to match a symbol
        bool matched = false;
        for (int sym = 0; sym < 256; ++sym)
        {
            int nb = kHuffTable[sym].bits;
            if (nb > bitsLeft)
                continue;
            uint64_t mask = (1ULL << nb) - 1;
            uint64_t code = (bits >> (bitsLeft - nb)) & mask;
            if (code == kHuffTable[sym].code)
            {
                result += static_cast<char>(sym);
                bitsLeft -= nb;
                matched = true;
                break;
            }
        }
        if (!matched)
        {
            // Remaining bits must be EOS padding (all 1s, at most 7 bits)
            if (bitsLeft > 7)
                throw std::runtime_error("HPACK: Huffman decode error");
            break;
        }
    }
    return result;
}

// ── HpackDecoder ──────────────────────────────────────────────────────────────

HpackDecoder::HpackDecoder(size_t maxTableSize)
    : table_(maxTableSize)
{
}

uint64_t HpackDecoder::decodeInt(const uint8_t * data, size_t len, size_t & pos, uint8_t prefixBits)
{
    uint64_t mask = (1u << prefixBits) - 1;
    uint64_t value = data[pos++] & mask;
    if (value < mask)
        return value;
    uint64_t m = 0;
    while (pos < len)
    {
        uint8_t b = data[pos++];
        value += uint64_t(b & 0x7f) << m;
        m += 7;
        if (!(b & 0x80))
            break;
    }
    return value;
}

std::string HpackDecoder::decodeStr(const uint8_t * data, size_t len, size_t & pos)
{
    if (pos >= len)
        throw std::runtime_error("HPACK: unexpected end of data");
    bool huffman = (data[pos] & 0x80) != 0;
    uint64_t strLen = decodeInt(data, len, pos, 7);
    if (pos + strLen > len)
        throw std::runtime_error("HPACK: string length exceeds buffer");
    std::string result;
    if (huffman)
        result = huffmanDecode(data + pos, strLen);
    else
        result.assign(reinterpret_cast<const char *>(data + pos), strLen);
    pos += strLen;
    return result;
}

void HpackDecoder::applyHeader(DecodedHeaders & out, std::string name, std::string value)
{
    if (name == ":method")
        out.method = std::move(value);
    else if (name == ":path")
        out.path = std::move(value);
    else if (name == ":scheme")
        out.scheme = std::move(value);
    else if (name == ":authority")
        out.authority = std::move(value);
    else if (name == ":status")
        out.status = std::move(value);
    else
    {
        http::HttpHeader hdr(name, value);
        out.headers.insert_or_assign(hdr.name(), std::move(hdr));
    }
}

DecodedHeaders HpackDecoder::decode(const uint8_t * data, size_t len)
{
    DecodedHeaders out;
    size_t pos = 0;

    while (pos < len)
    {
        uint8_t first = data[pos];

        if (first & 0x80)
        {
            // Indexed Header Field (RFC 7541 §6.1)
            uint64_t idx = decodeInt(data, len, pos, 7);
            if (idx == 0)
                throw std::runtime_error("HPACK: index 0 is invalid");
            const auto & entry = table_.get(idx);
            applyHeader(out, entry.name, entry.value);
        }
        else if ((first & 0xc0) == 0x40)
        {
            // Literal with Incremental Indexing (RFC 7541 §6.2.1)
            uint64_t idx = decodeInt(data, len, pos, 6);
            std::string name, value;
            if (idx == 0)
                name = decodeStr(data, len, pos);
            else
                name = table_.get(idx).name;
            value = decodeStr(data, len, pos);
            table_.insert(name, value);
            applyHeader(out, std::move(name), std::move(value));
        }
        else if ((first & 0xe0) == 0x20)
        {
            // Dynamic Table Size Update (RFC 7541 §6.3)
            uint64_t newSize = decodeInt(data, len, pos, 5);
            table_.setMaxSize(newSize);
        }
        else
        {
            // Literal without indexing / never indexed (RFC 7541 §6.2.2, §6.2.3)
            uint8_t prefixBits = ((first & 0xf0) == 0x10) ? 4 : 4;
            uint64_t idx = decodeInt(data, len, pos, prefixBits);
            std::string name, value;
            if (idx == 0)
                name = decodeStr(data, len, pos);
            else
                name = table_.get(idx).name;
            value = decodeStr(data, len, pos);
            applyHeader(out, std::move(name), std::move(value));
        }
    }
    return out;
}

// ── HpackEncoder ──────────────────────────────────────────────────────────────

HpackEncoder::HpackEncoder(size_t maxTableSize)
    : table_(maxTableSize)
{
}

void HpackEncoder::encodeInt(std::vector<uint8_t> & out, uint64_t value,
                             uint8_t prefixBits, uint8_t firstByte)
{
    uint64_t maxVal = (1u << prefixBits) - 1;
    if (value < maxVal)
    {
        out.push_back(firstByte | static_cast<uint8_t>(value));
        return;
    }
    out.push_back(firstByte | static_cast<uint8_t>(maxVal));
    value -= maxVal;
    while (value >= 0x80)
    {
        out.push_back(static_cast<uint8_t>((value & 0x7f) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

void HpackEncoder::encodeStr(std::vector<uint8_t> & out, std::string_view s)
{
    // Literal, no Huffman encoding
    encodeInt(out, s.size(), 7, 0x00);
    out.insert(out.end(), s.begin(), s.end());
}

std::vector<uint8_t> HpackEncoder::encodeResponse(const http::HttpResponse & resp)
{
    std::vector<uint8_t> out;
    out.reserve(128);

    std::string statusStr = std::to_string(resp.statusCode);
    int idx = table_.find(":status", statusStr);
    if (idx > 0)
    {
        encodeInt(out, idx, 7, 0x80);
    }
    else
    {
        int nameIdx = (idx < 0) ? -idx : 0;
        encodeInt(out, nameIdx, 6, 0x40);
        if (nameIdx == 0)
            encodeStr(out, ":status");
        encodeStr(out, statusStr);
        table_.insert(":status", statusStr);
    }

    for (const auto & [key, hdr] : resp.headers)
    {
        if (hdr.nameCode() == http::HttpHeader::NameCode::ContentLength
            || hdr.nameCode() == http::HttpHeader::NameCode::TransferEncoding
            || hdr.nameCode() == http::HttpHeader::NameCode::Connection)
            continue;

        std::string_view lname = hdr.name(); // already lowercase in our impl
        std::string_view val = hdr.value();

        int hidx = table_.find(lname, val);
        if (hidx > 0)
        {
            encodeInt(out, hidx, 7, 0x80);
        }
        else
        {
            int nidx = (hidx < 0) ? -hidx : 0;
            encodeInt(out, nidx, 6, 0x40);
            if (nidx == 0)
                encodeStr(out, lname);
            encodeStr(out, val);
            table_.insert(std::string(lname), std::string(val));
        }
    }

    if (resp.contentLength > 0)
    {
        std::string val = std::to_string(resp.contentLength);
        encodeInt(out, 0, 6, 0x40);
        encodeStr(out, "content-length");
        encodeStr(out, val);
    }

    for (const auto & cookie : resp.cookies)
    {
        std::string val = cookie.toString();
        encodeInt(out, 0, 6, 0x40);
        encodeStr(out, "set-cookie");
        encodeStr(out, val);
    }

    return out;
}

} // namespace nitrocoro::http2::hpack
