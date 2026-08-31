# Release Notes v2.0.10

**Release Date**: 2026-08-31  
**Version**: 2.0.10  
**Category**: Patch - Bug fixes, API resilience, and UI optimization

## Summary

Version 2.0.10 addresses critical JSON parsing errors observed on long-running dashboard sessions, hardens API response validation, and optimizes all browser tabs for better clarity and error reporting. All fixes maintain backward compatibility with existing features and preserve the secure server-side boundary for stock API access.

## Issues Fixed

### Dependency Management (v2.0.10+)

**Problem**: The nlohmann/json library was vendored in `third_party/nlohmann/` as a single header file, requiring manual updates and complicating dependency resolution across different build systems (CMake, Bazel).

**Fixes Applied**:
- Removed vendored `third_party/nlohmann/json.hpp` single-header copy
- Added `nlohmann_json/3.11.3` to `conanfile.py` requirements for proper dependency management
- Simplified CMakeLists.txt to use `find_package(nlohmann_json REQUIRED)` with Conan-provided package
- Removed complex fallback logic (vendored→system→FetchContent) in favor of simple Conan-based resolution
- Ensures consistent version across all platforms and build systems

**Benefits**:
- Cleaner build configuration with single source of truth in `conanfile.py`
- Automatic version management through Conan package manager
- Easier to maintain and upgrade dependencies in the future
- Reduced repository bloat by removing vendored headers
- Consistent behavior whether building with CMake or Bazel

### Critical: JSON Parsing Errors on Long-Running Sessions

**Problem**: Browser dashboard showed "JSON.parse: unexpected character at line 1 column 1" error on multiple tabs (Holdings, News, Alerts, Positions, JSON, Data health, Config) when the application ran for extended periods.

**Root Causes**:
- API endpoint responses were not validated before sending to the browser
- Empty or malformed JSON from fallback data sources was sent as-is
- No error handling for partial or incomplete JSON payloads
- JavaScript fetch().json() calls did not catch parsing exceptions
- Exception messages were exposed without sanitization

**Fixes Applied**:
- Added `ensureValidJson()` helper in C++ to validate all JSON responses before sending
- Wrapped all JSON-generating functions (`holdingsForUi`, `filteredNews`) with try-catch blocks
- Implemented proper fallback JSON for all edge cases (empty data, parse errors, timeout)
- Enhanced JavaScript `get()` function to catch JSON parse errors and log to console
- Updated error display to show actionable messages instead of raw exceptions
- All API endpoints now return valid JSON or descriptive error objects

### API Response Reliability

**Problem**: When underlying data sources failed, the API endpoints could return empty strings, incomplete JSON, or unhandled exceptions.

**Fixes Applied**:
- `/api/holdings` and `/api/positions` now wrap snapshot calls with error handling
- `/api/news` falls back to cached or local portfolio data when API fails
- `/api/config` returns JSON instead of raw string concatenation
- All endpoints validate output and return `{"status": "error", "details": "..."}` on failure
- JavaScript `get()` function provides default empty data for news API to prevent cascading failures

### Browser Error Reporting

**Problem**: Users saw "Unable to load this view: [error message]" without context or actionable guidance.

**Fixes Applied**:
- Enhanced `fail()` function displays a full error panel with title, message, and guidance
- Browser console logs detailed error context for developer debugging
- Error messages include the API endpoint and request that failed
- Status bar shows "Error: [message]" for quick visual feedback
- All tabs handle missing or malformed API data gracefully

## Features Added

### Data Health Tab

New operational diagnostics tab showing real-time portfolio health:
- **Holdings loaded**: count of stocks in current snapshot
- **News articles**: total filtered articles matching holdings
- **News decisions**: alert decisions generated from sentiment analysis
- **Missing prices**: holdings without valid price data
- Includes a health check table with status indicators (Healthy/Empty, Active/Quiet, Generated/Idle)

### Enhanced JSON Tab

Improved raw data inspection view:
- Clear "Runtime payload" label indicating live API response
- Explanatory note that this view exposes the raw server payload
- Pre-formatted JSON in a monospace view for easy inspection
- Error handling if API returns invalid JSON

### Updated Config Tab

Security-focused configuration reference:
- Shows holdings source (`config/holding.csv`)
- Shows news source (`config/portfolio_news.json`)
- Displays auth mode (Env + session)
- Confirms HTTPS-only security model
- Includes operational guidance about the approved configuration boundary

## UI Improvements

### Consistent Tab Rendering

- All nine dashboard tabs (Overview, News, Alerts, Deeper analysis, Fundamentals, Positions, JSON, Data health, Config) use a single canonical render path
- Eliminates duplicate DOM injection and stale UI conflicts
- Tabs automatically clear cache and refresh when switching views
- Loading states consistent across all views

### Error Messaging

- Error panel includes: title, detailed message, and copy-friendly formatting
- Browser console warnings logged for debugging
- Status bar updates with error context
- "Try refreshing the page" guidance provided to users

### Performance

- No new performance regressions
- JSON validation is fast and occurs server-side before network transmission
- Browser caching still works for healthy responses
- Failed requests are quickly identified and handled

## Security

### Maintained Boundaries

- All fixes maintain the secure server-side boundary for stock API access
- No changes to HTTPS-only enforcement or host allowlisting
- Environment secrets (FOLIO_LOGIN_CODE) are not exposed to browser
- Fallback data sources (local JSON files) remain protected

### Enhanced Validation

- All JSON responses validated before transmission to browser
- Error details sanitized to avoid exposing internal paths or structure
- JavaScript error handlers prevent malformed data from reaching UI logic
- No new attack surface introduced

## Testing

### Unit Tests

- All existing gtest regression tests pass (100%, 0 failures)
- Security validation tests for login code normalization and safe comparison
- No new test failures or warnings

### Manual Testing

- Tested error scenarios: empty API responses, malformed JSON, network timeouts
- Verified all nine tabs load and display correctly on first access
- Confirmed error messages appear when API endpoints return invalid data
- Verified error recovery: users can refresh and retry after errors

## Breaking Changes

**None**. All changes are backward compatible.

- Existing API response structure unchanged for valid data
- Browser cache behavior preserved
- Saved news fallback path unchanged
- Local-first architecture retained

## Deprecations

**None**.

## Migration Guide

**For Users**: 
- No action required
- Dashboard error messages will be clearer and more actionable
- Long-running sessions no longer show JSON parsing errors

**For Operators**:
- No configuration changes required
- Build and deploy using the same CMake or Bazel process
- Environment variables (FOLIO_LOGIN_CODE, ~/.upstox.env) work as before

**For Developers**:
- New `ensureValidJson()` helper available for future endpoint additions
- All JSON-generating functions follow the try-catch pattern for consistency
- Browser console logs detailed error context when debugging

## Code Quality

### Added/Modified Files

- `src/WebServer.cpp`: JSON validation, error handling, tab optimization
- `README.md`: Updated fix summary with detailed breakdown
- `design/RELEASE_NOTES_V2.0.10.md`: This document

### Static Analysis

- No new compiler warnings introduced
- All code follows existing style conventions
- Unused function warnings (from previous changes) do not affect functionality

## Deployment Notes

### Prerequisites

- CMake 3.20+ or Bazel (same as previous versions)
- C++17 compiler support
- No changes to dependency versions

### Build Command

```bash
cd /home/ritup2404/baran-capital-view
./buildCode.sh 1 --rebuild  # CMake build
# or
./buildCode.sh 2 --rebuild  # Bazel build
```

### Runtime Command

```bash
source ~/.upstox.env  # Load FOLIO_LOGIN_CODE and Upstox credentials
./run.sh --web        # Start dashboard
```

### Rollback

If needed, use the previous version (2.0.9 or earlier):
```bash
git checkout v2.0.9
./buildCode.sh 1 --rebuild
```

## Known Limitations

- Deeper analysis tab continues to refresh every 30 seconds (by design)
- News API fallback uses saved articles from last successful fetch
- Market status indicator is IST timezone only (for Upstox India)

## Future Work

- Consider adding per-tab error retry buttons in future minor release
- Explore advanced caching strategy for stable API response sets
- Investigate gRPC or Protocol Buffers for more efficient data transfer if JSON size becomes a bottleneck

## Acknowledgments

This release reflects fixes for long-running dashboard session stability and was developed while maintaining security and backward compatibility as core principles.

## Support

For issues or questions about this release:
1. Check the browser console for detailed error logs
2. Review the [Security Notes](SECURITY_SUMMARY.md) for API boundary documentation
3. See [Low Level Design](LowLevelDesign.md) for endpoint specifications

---

**End of Release Notes v2.0.10**
