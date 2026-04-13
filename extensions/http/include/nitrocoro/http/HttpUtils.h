#pragma once

#include <nitrocoro/http/HttpTypes.h>

#include <string_view>

namespace nitrocoro::http
{

const std::string_view & statusCodeToString(uint16_t code);

const std::string_view & versionToString(Version version);

} // namespace nitrocoro::http
