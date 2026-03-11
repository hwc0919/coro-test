#pragma once

/** @file UrlEncode.h
 *  @brief URL encoding and decoding utilities.
 */

#include <string>
#include <string_view>

namespace nitrocoro::utils
{

// Decode a full URL path. '/' is preserved as-is, '%2F' is NOT decoded.
// Invalid percent-encoded sequences are kept as-is.
std::string urlDecode(std::string_view input);

// Decode a single query string or form key/value component.
// Both '+' and '%20' are decoded to space. '%2B' is decoded to '+'.
// Invalid percent-encoded sequences are kept as-is.
std::string urlDecodeComponent(std::string_view input);

// Encode a full URL path. '/' is NOT encoded. Space -> '%20', '+' -> '%2B'.
std::string urlEncode(std::string_view input);

// Encode a single query string key/value component.
// Space -> '%20', '/' -> '%2F', '+' -> '%2B'.
std::string urlEncodeComponent(std::string_view input);

// Encode a single form (application/x-www-form-urlencoded) key/value component.
// Space -> '+', '/' -> '%2F', '+' -> '%2B'.
std::string formEncode(std::string_view input);

// Decode a single form key/value component. Forwards to urlDecodeComponent.
inline std::string formDecode(std::string_view input)
{
    return urlDecodeComponent(input);
}

} // namespace nitrocoro::utils
