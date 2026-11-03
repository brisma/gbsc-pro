#include <string.h>
#include <string>
#include <unity.h>
#include "http-parse.h"

void setUp(void) {}
void tearDown(void) {}

void test_url_split(void) {
    UrlParts u;
    TEST_ASSERT_TRUE(urlSplit("https://github.com/a/b", u));
    TEST_ASSERT_TRUE(u.tls);
    TEST_ASSERT_EQUAL_STRING("github.com", u.host);
    TEST_ASSERT_EQUAL_UINT16(443, u.port);
    TEST_ASSERT_EQUAL_STRING("/a/b", u.path);
    TEST_ASSERT_TRUE(urlSplit("http://x:8080/p", u));
    TEST_ASSERT_FALSE(u.tls);
    TEST_ASSERT_EQUAL_STRING("x", u.host);
    TEST_ASSERT_EQUAL_UINT16(8080, u.port);
    TEST_ASSERT_FALSE(urlSplit("https://github.com", u));     // no path
    TEST_ASSERT_FALSE(urlSplit("ftp://h/x", u));
    std::string longHost = "https://" + std::string(70, 'a') + "/x";
    TEST_ASSERT_FALSE(urlSplit(longHost.c_str(), u));
    TEST_ASSERT_FALSE(urlSplit("http://h:99999/x", u));
}

void test_status_and_headers(void) {
    TEST_ASSERT_EQUAL_INT(302, httpStatus("HTTP/1.1 302 Found"));
    TEST_ASSERT_EQUAL_INT(200, httpStatus("HTTP/1.0 200 OK"));
    TEST_ASSERT_EQUAL_INT(-1, httpStatus("garbage"));
    const char* v;
    TEST_ASSERT_TRUE(httpHeader("content-length: 42", "Content-Length", v));
    TEST_ASSERT_EQUAL_STRING("42", v);
    TEST_ASSERT_FALSE(httpHeader("Content-Type: x", "Content-Length", v));
    TEST_ASSERT_FALSE(httpHeader("Content-Lengthy: 1", "Content-Length", v));
}


// ---- Incremental response parser (OTA pull) ----
static std::string collected;
static bool collect(const uint8_t* d, size_t n, void*) { collected.append((const char*)d, n); return true; }

static std::string response(const char* head, size_t bodyLen) {
    std::string b;
    for (size_t i = 0; i < bodyLen; i++) b += (char)('a' + i % 26);
    return std::string(head) + b;
}

void test_response_split_everywhere(void) {
    std::string r = response("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: 300\r\n"
                             "Connection: close\r\n\r\n", 300);
    std::string body = r.substr(r.find("\r\n\r\n") + 4);
    for (size_t cut = 1; cut < r.size(); cut++) {
        HttpResponse h;
        TEST_ASSERT_TRUE(h.begin(1000));
        collected.clear();
        int a = h.feed((const uint8_t*)r.data(), cut, collect, nullptr);
        TEST_ASSERT_EQUAL_INT(HTTP_MORE, a);
        int b = h.feed((const uint8_t*)r.data() + cut, r.size() - cut, collect, nullptr);
        TEST_ASSERT_EQUAL_INT(HTTP_DONE, b);
        TEST_ASSERT_EQUAL_INT(200, h.status());
        TEST_ASSERT_EQUAL_UINT32(300, h.contentLength());
        TEST_ASSERT_TRUE(collected == body);
        h.end();
    }
}

static int feedAll(const std::string& r, uint32_t maxBody) {
    HttpResponse h;
    h.begin(maxBody);
    collected.clear();
    int v = h.feed((const uint8_t*)r.data(), r.size(), collect, nullptr);
    h.end();
    return v;
}

void test_response_lowercase_content_length(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_DONE, feedAll(response("HTTP/1.1 200 OK\r\ncontent-length: 5\r\n\r\n", 5), 100));
}

void test_response_404(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_E_STATUS, feedAll("HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\nnot found", 100));
}

void test_response_redirect_is_error(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_E_STATUS,
                          feedAll("HTTP/1.1 301 Moved\r\nLocation: https://x/\r\nContent-Length: 0\r\n\r\n", 100));
}

void test_response_no_length(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_E_LENGTH, feedAll("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello", 100));
}

void test_response_too_big(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_E_TOO_BIG, feedAll(response("HTTP/1.1 200 OK\r\nContent-Length: 101\r\n\r\n", 101), 100));
}

void test_response_extra_bytes(void) {
    TEST_ASSERT_EQUAL_INT(HTTP_E_EXTRA, feedAll(response("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", 6), 100));
}

void test_response_header_too_long(void) {
    std::string h = "HTTP/1.1 200 OK\r\nX-Long: " + std::string(300, 'x') + "\r\nContent-Length: 1\r\n\r\na";
    TEST_ASSERT_EQUAL_INT(HTTP_E_HEADER, feedAll(h, 100));
}

void test_response_short_body_not_done(void) {
    HttpResponse h;
    h.begin(100);
    std::string r = response("HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\n", 4);
    TEST_ASSERT_EQUAL_INT(HTTP_MORE, h.feed((const uint8_t*)r.data(), r.size(), collect, nullptr));
    TEST_ASSERT_EQUAL_UINT32(4, h.received());
    h.end();
}

static bool refuse(const uint8_t*, size_t, void*) { return false; }
void test_response_sink_abort(void) {
    HttpResponse h;
    h.begin(100);
    std::string r = response("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", 5);
    TEST_ASSERT_EQUAL_INT(HTTP_E_SINK, h.feed((const uint8_t*)r.data(), r.size(), refuse, nullptr));
    h.end();
}

void test_request_has_no_accept_encoding(void) {
    UrlParts u;
    TEST_ASSERT_TRUE(urlSplit("http://gbsc.brisma.it/v1/beta/manifest.txt", u));
    char req[256];
    TEST_ASSERT_TRUE(httpRequest(u, "GBSC-Pro/3.1.0-beta2", req, sizeof(req)));
    std::string r = req;
    TEST_ASSERT_EQUAL_INT(0, (int)r.find("GET /v1/beta/manifest.txt HTTP/1.0\r\nHost: gbsc.brisma.it\r\n"));
    TEST_ASSERT_TRUE(r.find("Connection: close\r\n") != std::string::npos);
    TEST_ASSERT_TRUE(r.find("User-Agent: GBSC-Pro/3.1.0-beta2\r\n") != std::string::npos);
    TEST_ASSERT_TRUE(r.find("Accept-Encoding") == std::string::npos);
    TEST_ASSERT_TRUE(r.size() >= 4 && r.compare(r.size() - 4, 4, "\r\n\r\n") == 0);
    char tiny[20];
    TEST_ASSERT_FALSE(httpRequest(u, "GBSC-Pro/3.1.0", tiny, sizeof(tiny)));
    UrlParts p8080;
    TEST_ASSERT_TRUE(urlSplit("http://host:8080/x", p8080));
    TEST_ASSERT_TRUE(httpRequest(p8080, "UA", req, sizeof(req)));
    TEST_ASSERT_TRUE(std::string(req).find("Host: host:8080\r\n") != std::string::npos);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_url_split);
    RUN_TEST(test_status_and_headers);
    RUN_TEST(test_response_split_everywhere);
    RUN_TEST(test_response_lowercase_content_length);
    RUN_TEST(test_response_404);
    RUN_TEST(test_response_redirect_is_error);
    RUN_TEST(test_response_no_length);
    RUN_TEST(test_response_too_big);
    RUN_TEST(test_response_extra_bytes);
    RUN_TEST(test_response_header_too_long);
    RUN_TEST(test_response_short_body_not_done);
    RUN_TEST(test_response_sink_abort);
    RUN_TEST(test_request_has_no_accept_encoding);
    return UNITY_END();
}
