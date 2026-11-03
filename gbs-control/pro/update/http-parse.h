// URL splitting and HTTP/1.x response parsing for the update client (pure, host-testable)
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef bool (*HttpSink)(const uint8_t* data, size_t len, void* ctx);   // false aborts the download

struct UrlParts { bool tls; char host[64]; uint16_t port; const char* path; };   // path points into the URL

bool urlSplit(const char* url, UrlParts& u);
int httpStatus(const char* statusLine);                                      // -1 when not a status line
bool httpHeader(const char* line, const char* name, const char*& value);    // case-insensitive name

// OTA pull: "GET <path> HTTP/1.0" with Host, User-Agent and Connection: close, never
// Accept-Encoding (CloudFront then keeps Content-Length); false when out cannot hold it
bool httpRequest(const UrlParts& u, const char* userAgent, char* out, size_t n);

// Incremental HTTP/1.x response: status line and headers in any split, then the body to the sink. Only a 200 with
// a Content-Length within maxBody is accepted; redirects are errors. The 256-byte header line buffer lives on the
// heap between begin() and end()
enum : int { HTTP_MORE = 0, HTTP_DONE = 1, HTTP_E_STATUS = -1, HTTP_E_LENGTH = -2, HTTP_E_TOO_BIG = -3,
             HTTP_E_HEADER = -4, HTTP_E_SINK = -5, HTTP_E_EXTRA = -6, HTTP_E_MEMORY = -7 };
class HttpResponse {
public:
    HttpResponse() = default;
    ~HttpResponse() { end(); }
    HttpResponse(const HttpResponse&) = delete;
    HttpResponse& operator=(const HttpResponse&) = delete;
    bool begin(uint32_t maxBody);                  // false: no memory for the line buffer
    void end();
    int feed(const uint8_t* d, size_t n, HttpSink sink, void* ctx);   // HTTP_MORE, HTTP_DONE or an HTTP_E_* code
    int status() const { return status_; }
    uint32_t contentLength() const { return clen_; }
    uint32_t received() const { return got_; }
private:
    static const uint16_t kLine = 256;
    char* line_ = nullptr;
    uint16_t lineLen_ = 0;
    uint32_t maxBody_ = 0, clen_ = 0, got_ = 0;
    int status_ = 0, result_ = HTTP_MORE;
    bool first_ = true, haveLen_ = false, body_ = false;
    int headerLine();                              // a complete line in line_: HTTP_MORE or an error
};

