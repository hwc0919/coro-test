#pragma once

#include <string>
#include <string_view>

namespace nitrocoro::utils
{

std::string base64Encode(std::string_view input);
std::string base64Decode(std::string_view input);

} // namespace nitrocoro::utils
