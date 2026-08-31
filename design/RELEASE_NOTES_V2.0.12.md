# Release Notes v2.0.12

**Version**: 2.0.12

## Summary

Version 2.0.12 removes duplicate Overview sorting controls and optimizes the shared dashboard filter behavior without changing Stock API access.

## UI Fixes

- Removed the redundant Overview `Confidence order` selector.
- Kept table-header arrow sorting as the single sorting interaction.
- Updated filtering to apply only to data rows, articles, and stock controls.
- Table headers remain visible while filtering.
- Existing tab navigation, refresh, detail popups, and serial numbering remain unchanged.

## Security

- Filtering and sorting operate on data already loaded in the browser.
- No user-controlled filter or sort value is sent to the Stock API.
- Existing server-side API access, HTTPS restrictions, session handling, and HTML escaping remain unchanged.

## Validation

- CMake build completed successfully.
- Existing C++ regression tests passed.
- No new external API calls were introduced by the UI changes.
