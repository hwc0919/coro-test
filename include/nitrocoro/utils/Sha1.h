#pragma once

#include <array>
#include <string>
#include <string_view>

namespace nitrocoro::utils
{

// Returns raw 20-byte digest
std::array<uint8_t, 20> sha1(std::string_view input);

// Returns lowercase hex string
std::string sha1Hex(std::string_view input);

} // namespace nitrocoro::utils
