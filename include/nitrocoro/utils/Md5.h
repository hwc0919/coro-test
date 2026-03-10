#pragma once

#include <array>
#include <string>
#include <string_view>

namespace nitrocoro::utils
{

// Returns raw 16-byte digest
std::array<uint8_t, 16> md5(std::string_view input);

// Returns lowercase hex string
std::string md5Hex(std::string_view input);

} // namespace nitrocoro::utils
