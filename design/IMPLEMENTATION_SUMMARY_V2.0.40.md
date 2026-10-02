# Baran Capital View v2.0.40 - Implementation Summary

## Change

Positions, Data health, and Config each occupied a separate primary navigation tab despite representing related operational tools.

## Implementation

- Reused the Positions, Data health, and Configuration view renderers inside a single Operations workspace.
- Added responsive, keyboard-accessible subview tabs and preserved the selected view during Operations refreshes.
- Prevented periodic live-data refresh while the read-only Configuration subview is selected.
- Added a versioned post-login popup summary and kept the existing authentication/API routes intact.

## Validation

- Added a test that checks the three subviews continue to route to their existing renderers.
- C++ and Python test suites remain the release gates.
