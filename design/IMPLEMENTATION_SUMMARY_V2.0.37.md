# Baran Capital View v2.0.37 - Implementation Summary

## Feature

The dashboard market indicator used only weekdays and fixed NSE hours, so it could report the market open on exchange holidays and did not account for special sessions.

## Implementation

- Added an Upstox client method for the documented current-year market-holidays endpoint.
- Validated response status, date shape, holiday types, and exchange arrays before accepting calendar data.
- Exposed the calendar through the session-protected web API and cached the last valid result for 12 hours.
- Updated the shared IST badge to show NSE trading holidays and respect NSE special-session timestamps; existing weekday/time behavior remains the fallback.
- Added the holiday feature to the versioned post-login popup.

## Validation

The C++ test suite includes holiday response validation for success, empty data, missing fields, invalid dates, and malformed JSON. Existing UI and Python regression workflows remain unchanged.
