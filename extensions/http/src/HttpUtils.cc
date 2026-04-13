#include <nitrocoro/http/HttpUtils.h>

namespace nitrocoro::http
{

const std::string_view & statusCodeToString(uint16_t code)
{
    switch (code)
    {
        case 100: {
            static std::string_view sv = "Continue";
            return sv;
        }
        case 101: {
            static std::string_view sv = "Switching Protocols";
            return sv;
        }
        case 102: {
            static std::string_view sv = "Processing";
            return sv;
        }
        case 103: {
            static std::string_view sv = "Early Hints";
            return sv;
        }
        case 200: {
            static std::string_view sv = "OK";
            return sv;
        }
        case 201: {
            static std::string_view sv = "Created";
            return sv;
        }
        case 202: {
            static std::string_view sv = "Accepted";
            return sv;
        }
        case 203: {
            static std::string_view sv = "Non-Authoritative Information";
            return sv;
        }
        case 204: {
            static std::string_view sv = "No Content";
            return sv;
        }
        case 205: {
            static std::string_view sv = "Reset Content";
            return sv;
        }
        case 206: {
            static std::string_view sv = "Partial Content";
            return sv;
        }
        case 207: {
            static std::string_view sv = "Multi-Status";
            return sv;
        }
        case 208: {
            static std::string_view sv = "Already Reported";
            return sv;
        }
        case 226: {
            static std::string_view sv = "IM Used";
            return sv;
        }
        case 300: {
            static std::string_view sv = "Multiple Choices";
            return sv;
        }
        case 301: {
            static std::string_view sv = "Moved Permanently";
            return sv;
        }
        case 302: {
            static std::string_view sv = "Found";
            return sv;
        }
        case 303: {
            static std::string_view sv = "See Other";
            return sv;
        }
        case 304: {
            static std::string_view sv = "Not Modified";
            return sv;
        }
        case 305: {
            static std::string_view sv = "Use Proxy";
            return sv;
        }
        case 306: {
            static std::string_view sv = "(Unused)";
            return sv;
        }
        case 307: {
            static std::string_view sv = "Temporary Redirect";
            return sv;
        }
        case 308: {
            static std::string_view sv = "Permanent Redirect";
            return sv;
        }
        case 400: {
            static std::string_view sv = "Bad Request";
            return sv;
        }
        case 401: {
            static std::string_view sv = "Unauthorized";
            return sv;
        }
        case 402: {
            static std::string_view sv = "Payment Required";
            return sv;
        }
        case 403: {
            static std::string_view sv = "Forbidden";
            return sv;
        }
        case 404: {
            static std::string_view sv = "Not Found";
            return sv;
        }
        case 405: {
            static std::string_view sv = "Method Not Allowed";
            return sv;
        }
        case 406: {
            static std::string_view sv = "Not Acceptable";
            return sv;
        }
        case 407: {
            static std::string_view sv = "Proxy Authentication Required";
            return sv;
        }
        case 408: {
            static std::string_view sv = "Request Time-out";
            return sv;
        }
        case 409: {
            static std::string_view sv = "Conflict";
            return sv;
        }
        case 410: {
            static std::string_view sv = "Gone";
            return sv;
        }
        case 411: {
            static std::string_view sv = "Length Required";
            return sv;
        }
        case 412: {
            static std::string_view sv = "Precondition Failed";
            return sv;
        }
        case 413: {
            static std::string_view sv = "Request Entity Too Large";
            return sv;
        }
        case 414: {
            static std::string_view sv = "Request-URI Too Large";
            return sv;
        }
        case 415: {
            static std::string_view sv = "Unsupported Media Type";
            return sv;
        }
        case 416: {
            static std::string_view sv = "Requested Range Not Satisfiable";
            return sv;
        }
        case 417: {
            static std::string_view sv = "Expectation Failed";
            return sv;
        }
        case 418: {
            static std::string_view sv = "I'm a Teapot";
            return sv;
        }
        case 421: {
            static std::string_view sv = "Misdirected Request";
            return sv;
        }
        case 422: {
            static std::string_view sv = "Unprocessable Entity";
            return sv;
        }
        case 423: {
            static std::string_view sv = "Locked";
            return sv;
        }
        case 424: {
            static std::string_view sv = "Failed Dependency";
            return sv;
        }
        case 425: {
            static std::string_view sv = "Too Early";
            return sv;
        }
        case 426: {
            static std::string_view sv = "Upgrade Required";
            return sv;
        }
        case 428: {
            static std::string_view sv = "Precondition Required";
            return sv;
        }
        case 429: {
            static std::string_view sv = "Too Many Requests";
            return sv;
        }
        case 431: {
            static std::string_view sv = "Request Header Fields Too Large";
            return sv;
        }
        case 451: {
            static std::string_view sv = "Unavailable For Legal Reasons";
            return sv;
        }
        case 500: {
            static std::string_view sv = "Internal Server Error";
            return sv;
        }
        case 501: {
            static std::string_view sv = "Not Implemented";
            return sv;
        }
        case 502: {
            static std::string_view sv = "Bad Gateway";
            return sv;
        }
        case 503: {
            static std::string_view sv = "Service Unavailable";
            return sv;
        }
        case 504: {
            static std::string_view sv = "Gateway Time-out";
            return sv;
        }
        case 505: {
            static std::string_view sv = "HTTP Version Not Supported";
            return sv;
        }
        case 506: {
            static std::string_view sv = "Variant Also Negotiates";
            return sv;
        }
        case 507: {
            static std::string_view sv = "Insufficient Storage";
            return sv;
        }
        case 508: {
            static std::string_view sv = "Loop Detected";
            return sv;
        }
        case 510: {
            static std::string_view sv = "Not Extended";
            return sv;
        }
        case 511: {
            static std::string_view sv = "Network Authentication Required";
            return sv;
        }
        default:
            if (code >= 100 && code < 200)
            {
                static std::string_view sv = "Informational";
                return sv;
            }
            else if (code >= 200 && code < 300)
            {
                static std::string_view sv = "Successful";
                return sv;
            }
            else if (code >= 300 && code < 400)
            {
                static std::string_view sv = "Redirection";
                return sv;
            }
            else if (code >= 400 && code < 500)
            {
                static std::string_view sv = "Bad Request";
                return sv;
            }
            else if (code >= 500 && code < 600)
            {
                static std::string_view sv = "Server Error";
                return sv;
            }
            else
            {
                static std::string_view sv = "Undefined Error";
                return sv;
            }
    }
}

const std::string_view & versionToString(Version version)
{
    switch (version)
    {
        case Version::kHttp10: {
            static std::string_view sv = "HTTP/1.0";
            return sv;
        }
        case Version::kHttp11: {
            static std::string_view sv = "HTTP/1.1";
            return sv;
        }
        default: {
            static std::string_view sv = "UNKNOWN";
            return sv;
        }
    }
}

} // namespace nitrocoro::http
