#pragma once

#include <string>
#include <string_view>

namespace nitrocoro::utils
{

std::string urlEncode(std::string_view input);
std::string urlDecode(std::string_view input);

// RFC 3986 component encoding: space -> %20, encodes all except unreserved chars
std::string urlEncodeComponent(std::string_view input);
std::string urlDecodeComponent(std::string_view input);

} // namespace nitrocoro::utils
