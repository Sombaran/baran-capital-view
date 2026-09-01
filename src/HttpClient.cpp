#include "HttpClient.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace folio {

namespace {

std::once_flag g_initFlag;

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    const size_t bytes = size * nmemb;
    out->append(ptr, bytes);
    return bytes;
}

// Read an environment variable, returning empty string when unset/blank.
std::string env(const char* name) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : std::string();
}

// First non-empty environment variable from the given list.
std::string firstEnv(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        std::string v = env(n);
        if (!v.empty()) return v;
    }
    return {};
}

std::string hostFromUrl(const std::string& url) {
    const std::string lower = [&url]() {
        std::string value = url;
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return value;
    }();

    const std::size_t schemePos = lower.find("://");
    if (schemePos == std::string::npos) {
        return {};
    }

    const std::size_t hostStart = schemePos + 3;
    std::size_t hostEnd = hostStart;
    while (hostEnd < lower.size() && lower[hostEnd] != '/' && lower[hostEnd] != '?' && lower[hostEnd] != '#') {
        ++hostEnd;
    }

    std::string host = lower.substr(hostStart, hostEnd - hostStart);
    if (host.empty() || host.find('@') != std::string::npos) {
        return {};
    }
    return host;
}

bool isAllowedUpstoxHost(const std::string& url) {
    const std::string host = hostFromUrl(url);
    if (host.empty()) {
        return false;
    }
    return host == "api.upstox.com" || host == "api-sandbox.upstox.com";
}

} // namespace

void CurlGlobal::ensureInit() {
    std::call_once(g_initFlag, []() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        std::atexit([]() { curl_global_cleanup(); });
    });
}

HttpClient::HttpClient() { CurlGlobal::ensureInit(); }
HttpClient::~HttpClient() = default;

HttpResponse HttpClient::get(const std::string& url,
                             const std::map<std::string, std::string>& headers,
                             long timeoutSeconds) const {
    HttpResponse result;
    const std::string lowered = [&url]() {
        std::string s = url;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return s;
    }();
    if (lowered.rfind("https://", 0) != 0) {
        result.error = "non-HTTPS URL blocked for stock API requests";
        return result;
    }
    if (!isAllowedUpstoxHost(url)) {
        result.error = "untrusted stock API host blocked";
        return result;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl_easy_init failed";
        return result;
    }

    struct curl_slist* hdrList = nullptr;
    for (const auto& kv : headers) {
        const std::string line = kv.first + ": " + kv.second;
        hdrList = curl_slist_append(hdrList, line.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
    // Never follow redirects: an upstream redirect must not move bearer
    // credentials or API traffic to an unapproved host.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    // Refresh resolver results during long-running dashboard sessions.
    curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 60L);
    if (hdrList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrList);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "portfolio-health/1.0");

    // --- TLS handshake robustness ----------------------------------------
    // "SSL connect error" is a handshake failure. Negotiating a modern TLS
    // version (1.2+) avoids failures against servers that reject older
    // protocols, which is the common cause on hardened RHEL hosts.
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

    // Some hosts publish an AAAA record whose IPv6 path is filtered while
    // IPv4 works (or vice-versa). FOLIO_FORCE_IPV4=1 / FOLIO_FORCE_IPV6=1
    // pins the address family to sidestep a broken route.
    if (!env("FOLIO_FORCE_IPV4").empty()) {
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    } else if (!env("FOLIO_FORCE_IPV6").empty()) {
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
    }

    // Custom CA bundle (some RHEL setups need an explicit path). curl also
    // honours CURL_CA_BUNDLE itself, but we accept a couple of common names.
    const std::string caBundle =
        firstEnv({"CURL_CA_BUNDLE", "SSL_CERT_FILE", "FOLIO_CA_BUNDLE"});
    if (!caBundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, caBundle.c_str());
    }
    const std::string caPath = firstEnv({"SSL_CERT_DIR", "FOLIO_CA_PATH"});
    if (!caPath.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAPATH, caPath.c_str());
    }

    // Explicit proxy support. libcurl reads *_proxy env vars automatically,
    // but wiring them in directly also covers the upper-case-only case and
    // makes NO_PROXY behave predictably in enterprise networks.
    const std::string proxy =
        firstEnv({"HTTPS_PROXY", "https_proxy", "ALL_PROXY", "all_proxy"});
    if (!proxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
    }
    const std::string noProxy = firstEnv({"NO_PROXY", "no_proxy"});
    if (!noProxy.empty()) {
        curl_easy_setopt(curl, CURLOPT_NOPROXY, noProxy.c_str());
    }

    // Never enable libcurl verbose tracing: it can print Authorization
    // headers and leak the bearer token into terminal or service logs.

    // Escape hatch for TLS-intercepting proxies with an untrusted CA.
    // Opt-in only (FOLIO_INSECURE=1) and clearly insecure — prefer adding
    // the proxy's CA to the trust store instead.
    if (!env("FOLIO_INSECURE").empty()) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode rc = CURLE_OK;
    constexpr int maxTransportAttempts = 3;
    for (int attempt = 1; attempt <= maxTransportAttempts; ++attempt) {
        result.body.clear();
        result.error.clear();
        result.statusCode = 0;
        rc = curl_easy_perform(curl);
        if (rc == CURLE_OK ||
            (rc != CURLE_COULDNT_RESOLVE_HOST && rc != CURLE_COULDNT_CONNECT &&
             rc != CURLE_OPERATION_TIMEDOUT)) {
            break;
        }
        if (attempt < maxTransportAttempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250 * attempt));
        }
    }
    if (rc != CURLE_OK) {
        result.error = curl_easy_strerror(rc);
        if (rc == CURLE_SSL_CONNECT_ERROR || rc == CURLE_SSL_CACERT ||
            rc == CURLE_PEER_FAILED_VERIFICATION) {
            result.error +=
                " (TLS handshake failed. If you are behind a proxy set "
                "HTTPS_PROXY; if your host uses a custom CA set "
                "CURL_CA_BUNDLE and inspect the HTTP status for details.)";
        }
        if (result.error.find("token") != std::string::npos ||
            result.error.find("unauthorized") != std::string::npos ||
            result.error.find("forbidden") != std::string::npos) {
            std::cerr << "Backend: Upstox access token stale or expired while calling "
                      << url << " :: " << result.error << "\n";
        }
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.statusCode);
    }

    if (hdrList) curl_slist_free_all(hdrList);
    curl_easy_cleanup(curl);
    return result;
}

} // namespace folio
