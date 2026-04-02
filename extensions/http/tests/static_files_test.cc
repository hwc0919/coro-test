/**
 * @file static_files_test.cc
 * @brief Integration tests for staticFiles() handler.
 */
#include <nitrocoro/http/HttpClient.h>
#include <nitrocoro/http/HttpServer.h>
#include <nitrocoro/http/StaticFiles.h>
#include <nitrocoro/net/TcpConnection.h>
#include <nitrocoro/testing/Test.h>
#include <nitrocoro/utils/Format.h>

#include <atomic>
#include <fcntl.h>
#include <filesystem>
#include <fstream>

using namespace nitrocoro;
using namespace nitrocoro::http;
namespace fs = std::filesystem;

// ── Helpers ───────────────────────────────────────────────────────────────────

struct TempDir
{
    fs::path path;
    TempDir()
    {
        static std::atomic<int> counter{ 0 };
        path = fs::temp_directory_path() / ("nitrocoro_test_" + std::to_string(counter++));
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }

    void write(const fs::path & rel, std::string_view content) const
    {
        auto full = path / rel;
        fs::create_directories(full.parent_path());
        std::ofstream(full) << content;
    }
};

static SharedFuture<> start_server(HttpServer & server)
{
    Scheduler::current()->spawn([&server]() -> Task<> { co_await server.start(); });
    return server.started();
}

static Task<std::string> rawHttp(uint16_t port, std::string req)
{
    auto conn = co_await net::TcpConnection::connect(net::InetAddress("127.0.0.1", port));
    co_await conn->write(req.data(), req.size());
    co_await conn->shutdown();
    std::string resp;
    char buf[4096];
    while (true)
    {
        size_t n = co_await conn->read(buf, sizeof(buf));
        if (n == 0)
            break;
        resp.append(buf, n);
    }
    co_return resp;
}

static int statusCode(const std::string & resp)
{
    auto pos = resp.find(' ');
    if (pos == std::string::npos)
        return 0;
    return std::stoi(resp.substr(pos + 1, 3));
}

static std::string getHeader(const std::string & resp, std::string_view name)
{
    std::string lower = resp;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    std::string lname(name);
    std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
    auto pos = lower.find(lname + ": ");
    if (pos == std::string::npos)
        return {};
    pos += lname.size() + 2;
    auto end = resp.find("\r\n", pos);
    return resp.substr(pos, end - pos);
}

// ── Tests ─────────────────────────────────────────────────────────────────────

/** GET existing file → 200 with correct body and Content-Type. */
NITRO_TEST(static_files_serve_file)
{
    TempDir dir;
    dir.write("hello.txt", "hello world");

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/hello.txt", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "hello world");
    NITRO_CHECK(resp.getHeader("content-type").find("text/plain") != std::string::npos);

    co_await server.stop();
}

/** GET non-existent file → 404. */
NITRO_TEST(static_files_not_found)
{
    TempDir dir;

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/missing.txt", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k404NotFound);

    co_await server.stop();
}

/** Path traversal attempt → 403. */
NITRO_TEST(static_files_path_traversal)
{
    TempDir dir;

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/../../etc/passwd", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k403Forbidden);

    co_await server.stop();
}

/** Second GET with matching ETag → 304 Not Modified. */
NITRO_TEST(static_files_etag_304)
{
    TempDir dir;
    dir.write("data.txt", "some content");

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    uint16_t port = server.listeningPort();
    HttpClient client(utils::format("http://127.0.0.1:{}", port));

    auto resp1 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp1.statusCode(), StatusCode::k200OK);
    const std::string & etag = resp1.getHeader("etag");
    NITRO_REQUIRE(!etag.empty());

    std::string req = "GET /data.txt HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "If-None-Match: "
                      + etag + "\r\n"
                               "Connection: close\r\n\r\n";
    auto resp2 = co_await rawHttp(port, req);
    NITRO_CHECK_EQ(statusCode(resp2), 304);

    co_await server.stop();
}

/** Second GET with matching Last-Modified → 304 Not Modified. */
NITRO_TEST(static_files_last_modified_304)
{
    TempDir dir;
    dir.write("data.txt", "some content");

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    uint16_t port = server.listeningPort();
    HttpClient client(utils::format("http://127.0.0.1:{}", port));

    auto resp1 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp1.statusCode(), StatusCode::k200OK);
    const std::string & lm = resp1.getHeader("last-modified");
    NITRO_REQUIRE(!lm.empty());

    std::string req = "GET /data.txt HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "If-Modified-Since: "
                      + lm + "\r\n"
                             "Connection: close\r\n\r\n";
    auto resp2 = co_await rawHttp(port, req);
    NITRO_CHECK_EQ(statusCode(resp2), 304);

    co_await server.stop();
}

/** HEAD request → 200 with headers, empty body. */
NITRO_TEST(static_files_head)
{
    TempDir dir;
    dir.write("page.html", "<h1>hi</h1>");

    HttpServer server(0);
    server.route("/*path", { "GET", "HEAD" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));
    ClientRequest headReq;
    headReq.setMethod(methods::Head);
    headReq.setPath("/page.html");
    auto headResp = co_await client.request(std::move(headReq));
    auto resp = co_await headResp.toCompleteResponse();
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK(resp.body().empty());
    NITRO_CHECK(!resp.getHeader("content-length").empty());
    NITRO_CHECK(resp.getHeader("content-type").find("text/html") != std::string::npos);

    co_await server.stop();
}

/** GET / → index.html; works with both /*path and /* (anonymous wildcard). */
NITRO_TEST(static_files_root_and_wildcard_variants)
{
    TempDir dir;
    dir.write("hello.txt", "hello");
    dir.write("index.html", "<h1>root</h1>");

    // named wildcard
    {
        HttpServer server(0);
        server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
        co_await start_server(server);
        HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));

        auto r1 = co_await client.get("/");
        NITRO_CHECK_EQ(r1.statusCode(), StatusCode::k200OK);
        NITRO_CHECK_EQ(r1.body(), "<h1>root</h1>");

        auto r2 = co_await client.get("/hello.txt");
        NITRO_CHECK_EQ(r2.statusCode(), StatusCode::k200OK);
        NITRO_CHECK_EQ(r2.body(), "hello");
        co_await server.stop();
    }

    // anonymous wildcard
    {
        HttpServer server(0);
        server.route("/*", { "GET" }, staticFiles(dir.path.string()));
        co_await start_server(server);
        HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));

        auto r1 = co_await client.get("/");
        NITRO_CHECK_EQ(r1.statusCode(), StatusCode::k200OK);
        NITRO_CHECK_EQ(r1.body(), "<h1>root</h1>");

        auto r2 = co_await client.get("/hello.txt");
        NITRO_CHECK_EQ(r2.statusCode(), StatusCode::k200OK);
        NITRO_CHECK_EQ(r2.body(), "hello");
        co_await server.stop();
    }
}

/** Mounted at /static/*path → serves files under that prefix. */
NITRO_TEST(static_files_subpath_mount)
{
    TempDir dir;
    dir.write("hello.txt", "hello");

    HttpServer server(0);
    server.route("/static/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/static/hello.txt", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.body(), "hello");

    co_await server.stop();
}

/** Cache-Control: max-age is set when maxAge > 0. */
NITRO_TEST(static_files_cache_control_max_age)
{
    TempDir dir;
    dir.write("style.css", "body{}");

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string(), { .max_age = 3600 }));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/style.css", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK(resp.getHeader("cache-control").find("max-age=3600") != std::string::npos);

    co_await server.stop();
}

/** GET with Accept-Encoding: gzip → serves .gz, Content-Encoding: gzip. */
NITRO_TEST(static_files_precompressed_gzip)
{
    TempDir dir;
    dir.write("app.js", "console.log('hello')");
    static constexpr unsigned char kGzip[] = {
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x4b, 0xce, 0xcf, 0x2b, 0xce, 0xcf, 0x49, 0xd5, 0x51, 0x48,
        0x54, 0xc8, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x27, 0x00, 0x00,
        0x00, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    dir.write("app.js.gz", std::string_view(reinterpret_cast<const char *>(kGzip), sizeof(kGzip)));

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    uint16_t port = server.listeningPort();
    std::string req = "GET /app.js HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Accept-Encoding: gzip\r\n"
                      "Connection: close\r\n\r\n";
    auto resp = co_await rawHttp(port, req);
    NITRO_CHECK_EQ(statusCode(resp), 200);
    NITRO_CHECK_EQ(getHeader(resp, "Content-Encoding"), "gzip");
    NITRO_CHECK_EQ(getHeader(resp, "Content-Type"), "text/javascript; charset=utf-8");
    NITRO_CHECK_EQ(getHeader(resp, "Content-Length"), std::to_string(sizeof(kGzip)));

    co_await server.stop();
}

/** Accept-Encoding order is respected: br listed first → serves .br. */
NITRO_TEST(static_files_precompressed_accept_order)
{
    TempDir dir;
    dir.write("app.js", "console.log('hello')");
    static constexpr unsigned char kGzip[] = {
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x4b, 0xce, 0xcf, 0x2b, 0xce, 0xcf, 0x49, 0xd5, 0x51, 0x48,
        0x54, 0xc8, 0x48, 0xcd, 0xc9, 0xc9, 0x57, 0x27, 0x00, 0x00,
        0x00, 0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    static constexpr unsigned char kBr[] = { 0x3b };
    dir.write("app.js.gz", std::string_view(reinterpret_cast<const char *>(kGzip), sizeof(kGzip)));
    dir.write("app.js.br", std::string_view(reinterpret_cast<const char *>(kBr), sizeof(kBr)));

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    uint16_t port = server.listeningPort();
    std::string req = "GET /app.js HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Accept-Encoding: br, gzip\r\n"
                      "Connection: close\r\n\r\n";
    auto resp = co_await rawHttp(port, req);
    NITRO_CHECK_EQ(statusCode(resp), 200);
    NITRO_CHECK_EQ(getHeader(resp, "Content-Encoding"), "br");
    NITRO_CHECK_EQ(getHeader(resp, "Content-Length"), std::to_string(sizeof(kBr)));

    co_await server.stop();
}

/** Unknown extension → Content-Type: application/octet-stream. */
NITRO_TEST(static_files_unknown_mime_type)
{
    TempDir dir;
    dir.write("data.xyz", "hello");

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/data.xyz", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.getHeader("content-type"), "application/octet-stream");

    co_await server.stop();
}

/** Custom mime type is used for registered extension. */
NITRO_TEST(static_files_custom_mime_type)
{
    TempDir dir;
    dir.write("data.custom", "hello");

    auto opts = StaticFilesOptions{};
    opts.mime_types[".custom"] = "application/x-custom";

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string(), std::move(opts)));
    co_await start_server(server);

    auto resp = co_await get(utils::format("http://127.0.0.1:{}/data.custom", server.listeningPort()));
    NITRO_CHECK_EQ(resp.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp.getHeader("content-type"), "application/x-custom");

    co_await server.stop();
}

/** Unregistered or disabled encoding → original file served, no Content-Encoding. */
NITRO_TEST(static_files_encoding_not_used)
{
    TempDir dir;
    dir.write("app.js", "console.log('hello')");
    static constexpr unsigned char kBr[] = { 0x3b };
    dir.write("app.js.br", std::string_view(reinterpret_cast<const char *>(kBr), sizeof(kBr)));

    {
        HttpServer server(0);
        server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
        co_await start_server(server);
        uint16_t port = server.listeningPort();
        std::string req = "GET /app.js HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept-Encoding: zstd\r\nConnection: close\r\n\r\n";
        auto resp = co_await rawHttp(port, req);
        NITRO_CHECK_EQ(statusCode(resp), 200);
        NITRO_CHECK(getHeader(resp, "Content-Encoding").empty());
        co_await server.stop();
    }

    {
        auto opts = StaticFilesOptions{};
        opts.accept_encodings = { { "gzip", "gz" } };
        HttpServer server(0);
        server.route("/*path", { "GET" }, staticFiles(dir.path.string(), std::move(opts)));
        co_await start_server(server);
        uint16_t port = server.listeningPort();
        std::string req = "GET /app.js HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept-Encoding: gzip, br\r\nConnection: close\r\n\r\n";
        auto resp = co_await rawHttp(port, req);
        NITRO_CHECK_EQ(statusCode(resp), 200);
        NITRO_CHECK(getHeader(resp, "Content-Encoding") != "br");
        co_await server.stop();
    }
}

NITRO_TEST(static_files_cache_hit)
{
    TempDir dir;
    dir.write("data.txt", "hello");

    StaticFilesOptions opts;
    opts.cache_ttl = 60;
    opts.cache_header = "X-Cache";

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string(), std::move(opts)));
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));

    auto resp1 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp1.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp1.getHeader("x-cache"), "MISS");

    auto resp2 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp2.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp2.getHeader("x-cache"), "HIT");
    NITRO_CHECK_EQ(resp2.body(), "hello");

    co_await server.stop();
}

/** Cache stale: file modified (new mtime/etag) → cache invalidated, new content served. */
NITRO_TEST(static_files_cache_stale)
{
    TempDir dir;
    dir.write("data.txt", "original");

    StaticFilesOptions opts;
    opts.cache_ttl = 60;
    opts.cache_header = "X-Cache";

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string(), std::move(opts)));
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));

    auto resp1 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp1.body(), "original");
    NITRO_CHECK_EQ(resp1.getHeader("x-cache"), "MISS");

    co_await nitrocoro::sleep(std::chrono::seconds(1));
    dir.write("data.txt", "updated");

    auto resp2 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp2.statusCode(), StatusCode::k200OK);
    NITRO_CHECK_EQ(resp2.body(), "updated");
    NITRO_CHECK_EQ(resp2.getHeader("x-cache"), "MISS");

    co_await server.stop();
}

/** File exceeding cache_max_file_size is not cached: always reads from disk. */
NITRO_TEST(static_files_cache_max_file_size)
{
    TempDir dir;
    dir.write("big.txt", std::string(10, 'x'));

    StaticFilesOptions opts;
    opts.cache_ttl = 60;
    opts.cache_max_file_size = 5;

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string(), std::move(opts)));
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));

    auto resp1 = co_await client.get("/big.txt");
    NITRO_CHECK_EQ(resp1.body(), std::string(10, 'x'));

    dir.write("big.txt", std::string(10, 'y'));

    auto resp2 = co_await client.get("/big.txt");
    NITRO_CHECK_EQ(resp2.body(), std::string(10, 'y'));

    co_await server.stop();
}

/** cache_ttl=0 (default) disables cache: file changes are always reflected. */
NITRO_TEST(static_files_cache_disabled)
{
    TempDir dir;
    dir.write("data.txt", "v1");

    HttpServer server(0);
    server.route("/*path", { "GET" }, staticFiles(dir.path.string()));
    co_await start_server(server);

    HttpClient client(utils::format("http://127.0.0.1:{}", server.listeningPort()));

    auto resp1 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp1.body(), "v1");

    dir.write("data.txt", "v2");

    auto resp2 = co_await client.get("/data.txt");
    NITRO_CHECK_EQ(resp2.body(), "v2");

    co_await server.stop();
}

int main(int argc, char ** argv)
{
    return nitrocoro::test::run_all(argc, argv);
}
