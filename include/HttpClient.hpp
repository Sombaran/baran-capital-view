#pragma once

#include <map>
#include <string>

namespace folio {

struct HttpResponse {
    long statusCode = 0;
    std::string body;
    std::string error; // non-empty on transport-level failure

    bool ok() const {
        return error.empty() && statusCode >= 200 && statusCode < 300;
    }
};

// Thin libcurl wrapper. Each request uses its own easy handle so instances
// are safe to share across threads.
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse get(const std::string& url,
                     const std::map<std::string, std::string>& headers,
                     long timeoutSeconds = 30) const;
};

// Ensures curl_global_init runs once per process.
class CurlGlobal {
public:
    static void ensureInit();
};

} // namespace folio
