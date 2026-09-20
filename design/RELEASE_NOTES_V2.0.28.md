# Release Notes v2.0.28

**Version:** 2.0.28  
**Date:** September 2, 2026

## Global market news reliability and error handling

- **Fixed global news error handling**: The `/api/global-news` endpoint now validates the news category and provides descriptive error messages for access token, network, or API failures.
- **Enhanced error diagnostics**: HTTP 401/403 errors explicitly prompt token renewal instead of generic "unavailable" messages. Network timeouts and DNS failures are distinguished from authorization issues.
- **Category input validation**: News categories are now validated to prevent injection and limit to 32 characters with lowercase alphanumeric characters and underscores only.
- **Response structure validation**: The backend verifies that successful Upstox responses contain the required `data` field before returning them to the browser.

The global market news tab now shows actionable error messages when the Upstox API is unreachable, the access token has expired, or the news feed is temporarily unavailable. The UI can therefore guide users to renew their token or check network connectivity.

## Unit test coverage improvements

- Added comprehensive test coverage for global news API error handling.
- Added validation tests for Upstox category input sanitization.
- Added tests for token expiration (HTTP 401) and authorization (HTTP 403) scenarios.
- Added JSON parse error handling tests for malformed Upstox responses.

Stock API credentials remain server-side. HTTPS, trusted-host validation, and secure session cookies are unchanged.

## Dashboard optimization

- Improved responsive navigation layout for consistent rendering on mobile and desktop.
- Optimized CSS for better browser performance and reduced paint time.
- Reduced inline JavaScript bundle size by extracting reusable functions.
- Added browser-level caching directives to prevent redundant API polling.

## Fix summary in release popup

The right-side release notes popup now includes a dedicated section summarizing all fixes applied in this patch. Users see:
- Global market news error handling improvements
- Unit test coverage additions
- Performance and UI optimization details
- Security boundaries and token handling changes

The popup is shown once per version and can be dismissed. It stores the version in localStorage to avoid showing the same popup repeatedly.

## Security and stability

- Input validation now prevents category injection attacks.
- Error messages are descriptive without exposing raw API responses.
- Backend logging provides operational visibility without leaking credentials.
- Existing HTTPS, token, and session security remain unchanged.
- All changes maintain backward compatibility with existing portfolio data and news workflows.

## Backward compatibility

All changes in 2.0.28 are fully backward compatible. Existing configurations, saved news, and holdings data remain functional. The global news API now fails gracefully when unavailable instead of rendering a generic error.
