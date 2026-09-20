# Baran Capital View v2.0.32 - Implementation Summary

## Security fixes

- RSI period validation now rejects zero, oversized, and overflow-sized values
  before any `period + 1` calculation.
- Market quote and OHLC routes validate instrument-key format and cap batches at
  500 keys.
- Python analysis input/output files are created with `mkstemp`, avoiding
  predictable `/tmp` names.
- Stock analysis and fundamentals caches are bounded to 64 completed entries.
- Dynamic browser links are restricted to HTTP(S) schemes after each render.

## Compatibility

The valid request and response contracts remain unchanged. The security changes
only reject malformed, oversized, or unsafe input before it reaches the broker
API or browser navigation.

## Validation

The C++ unit suite and Python regression suite are the release gate. The new
RSI boundary test protects the previously unsafe integer-overflow path.
