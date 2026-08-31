# Release Notes v2.0.14

**Version**: 2.0.14

## Summary

Version 2.0.14 adds automated regression testing to both supported build paths.

## Build and Test Workflow

- CMake builds run `ctest --test-dir build --output-on-failure` followed by `pytest -q`.
- Bazel builds run the native `//:portfolio_health_tests` target followed by `pytest -q`.
- `--skip-tests` is available for build-only workflows.
- Bazel now includes the existing C++ security and portfolio regression source as a native `cc_test` target.
- `pytest.ini` restricts Python discovery to project tests and excludes generated Bazel trees.

## Security and Compatibility

- Tests run after compilation and do not change runtime API behavior.
- Stock API credentials remain server-side and are not passed to test commands.
- Existing CMake, Bazel, UI, and CLI targets remain available.

## Validation

Expected results are one passing C++ test target and seven passing Python tests in the current repository.
