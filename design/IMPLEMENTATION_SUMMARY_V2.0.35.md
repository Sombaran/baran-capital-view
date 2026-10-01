# Baran Capital View v2.0.35 - Implementation Summary

## Problem

The Overview calculated Day P&L only from the optional `day_change` field. If the live holdings payload omitted it, the displayed value became zero despite live prices being available. The local CSV fallback exposed a portfolio-total Day P&L as a per-share change, causing quantity to be multiplied twice.

## Fix

- Added shared position helpers for per-unit day change and total position Day P&L.
- Normalize live holdings from last and close prices, and correct the local CSV fallback's per-unit representation.
- Added responsive shared sizing for dashboard metric grids, news lists, and wide tables.
- Added a right-side post-login popup addendum describing the v2.0.35 changes.

## Validation

- `portfolio_health_tests` passes, including a regression assertion for a 1,100 INR aggregate Day P&L.
- The existing browser API fields, login flow, and CSV schema remain compatible.
