# Baran Capital View v2.0.36 - Implementation Summary

## Problem

The dashboard displayed `#` as the serial-number column heading, which was unclear in the holdings table and inconsistent with the requested `S.No` label.

## Fix

- Normalize serial-number headers to `S.No` across rendered dashboard tables.
- Update Overview and deeper-analysis insertion logic to recognize the label and avoid duplicate columns.
- Add a post-login popup addendum and align release metadata on `2.0.36`.

## Validation

- C++ and Python regression suites remain the release gate.
- Serial-number columns remain separate from sortable data columns.
- No broker API, credential, or authentication contracts changed.
