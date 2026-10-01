# Release Notes v2.0.35

**Version:** 2.0.35  
**Date:** October 1, 2026

## Live Day P&L

- Fixed the Overview Day P&L staying at zero when live holdings omit `day_change` by deriving the per-unit change from live last and close prices.
- Corrected local CSV fallback values so a portfolio-total Day P&L is not multiplied by holding quantity a second time.
- Centralized per-position day-change and day-P&L calculations and added a regression assertion for the portfolio total.

## Dashboard layout

- Improved responsive metric grids and news layouts across all tabs.
- Kept wide tables inside horizontally scrollable containers and constrained content on narrow screens.

## Release transparency

- Added the v2.0.35 fix summary to the right-side popup shown after login.
- Aligned CMake, Conan, Bazel, README, and design metadata on `2.0.35`.

## Compatibility

The change preserves existing API response fields, login behavior, saved portfolio files, and local CSV format.
