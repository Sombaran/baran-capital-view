#!/bin/bash
# load_config.sh — Validate the environment-based Upstox credential
#
# Usage:
#   export UPSTOX_ACCESS_TOKEN='...'
#   . ./load_config.sh
#
# After sourcing, UPSTOX_ACCESS_TOKEN is exported and ready to use:
#   . ./load_config.sh && ./build/portfolio_health

set -e

if [[ -z "${UPSTOX_ACCESS_TOKEN:-}" ]]; then
    echo "Error: UPSTOX_ACCESS_TOKEN is not set" >&2
    return 1 2>/dev/null || exit 1
fi

echo "UPSTOX_ACCESS_TOKEN is set (${#UPSTOX_ACCESS_TOKEN} chars)"
