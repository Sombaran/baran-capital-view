# Release Notes v2.0.16

**Version**: 2.0.16

## Summary

Version 2.0.16 improves the Deeper analysis category panel by making each category's stock membership easy to inspect.

## Deeper Analysis Improvements

- Hovering over `No recent news`, `Neutral news`, `going good`, `invest more`, or `sell it off` shows the associated stock names.
- Keyboard focus shows the same stock list and exposes an accessible label.
- Existing click behavior continues to show the selected category below the controls.
- Empty categories clearly show `No stocks`.
- Names are taken from the existing category data and no additional API call is made.

## Security

- The feature is client-side only and does not send category values to the Stock API.
- Existing escaping, server-side credentials, HTTPS, and session controls remain unchanged.

## Validation

- Existing CMake and Bazel regression workflows remain unchanged.
- Existing C++ and Python tests should pass before deployment.
