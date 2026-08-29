#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

# Credentials must be injected by the shell, ~/.upstox.env, or a secret manager.
if [[ ( -z "${UPSTOX_API_KEY:-}" || -z "${UPSTOX_API_SECRET:-}" ||
  -z "${UPSTOX_ACCESS_TOKEN:-}" ) && -f "${HOME}/.upstox.env" ]]; then
  umask 077
  . "${HOME}/.upstox.env"
fi

# Load the local web login code once, without putting it in source control.
if [[ -z "${FOLIO_LOGIN_CODE:-}" && -f .folio_login_code ]]; then
  FOLIO_LOGIN_CODE=$(<.folio_login_code)
  export FOLIO_LOGIN_CODE
fi

# 1. Build (Release)
if [[ ! -x build/portfolio_health || "${1:-}" == "--rebuild" ]]; then
  ./buildCode.sh "${1:-}"
  [[ "${1:-}" == "--rebuild" ]] && shift || true
fi

# 2. Launch the interactive UI when no command-line action is supplied.
#    Explicit arguments remain available for scripts and automation.
#
#    Priority:
#      a) explicit CLI args     -> forwarded to portfolio_health
#      b) MY_PORTFOLIO env var  -> --portfolio "$MY_PORTFOLIO"
#      c) config/my_portfolio.csv -> --portfolio config/my_portfolio.csv
#      d) UPSTOX_API_KEY, UPSTOX_API_SECRET, UPSTOX_ACCESS_TOKEN -> live account
#      e) fallback              -> --file config/sample_positions.json
#
if [[ $# -gt 0 ]]; then
  exec ./build/portfolio_health "$@"
fi

exec ./build/portfolio_health
