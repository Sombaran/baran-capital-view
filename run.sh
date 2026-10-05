#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

# Credentials supplied by the parent process take precedence over the local fallback file.
if [[ ( -z "${UPSTOX_API_KEY:-}" || -z "${UPSTOX_API_SECRET:-}" ||
  -z "${UPSTOX_ACCESS_TOKEN:-}" ) && -f "${HOME}/.upstox.env" ]]; then
  provided_upstox_api_key="${UPSTOX_API_KEY:-}"
  provided_upstox_api_secret="${UPSTOX_API_SECRET:-}"
  provided_upstox_access_token="${UPSTOX_ACCESS_TOKEN:-}"
  umask 077
  . "${HOME}/.upstox.env"
  [[ -z "$provided_upstox_api_key" ]] || export UPSTOX_API_KEY="$provided_upstox_api_key"
  [[ -z "$provided_upstox_api_secret" ]] || export UPSTOX_API_SECRET="$provided_upstox_api_secret"
  [[ -z "$provided_upstox_access_token" ]] || export UPSTOX_ACCESS_TOKEN="$provided_upstox_access_token"
  unset provided_upstox_api_key provided_upstox_api_secret provided_upstox_access_token
fi

# Load the local web login code once, without putting it in source control.
if [[ -z "${FOLIO_LOGIN_CODE:-}" && -f .folio_login_code ]]; then
  FOLIO_LOGIN_CODE=$(<.folio_login_code)
  export FOLIO_LOGIN_CODE
fi

# 1. Build when the executable is missing or source/build metadata changed.
executable="build/portfolio_health"
needs_build=0
if [[ ! -x "$executable" ]]; then
  needs_build=1
elif [[ "${1:-}" == "--rebuild" ||
        run.sh -nt "$executable" ||
        buildCode.sh -nt "$executable" ||
        CMakeLists.txt -nt "$executable" ||
        conanfile.py -nt "$executable" ||
        MODULE.bazel -nt "$executable" ||
        BUILD.bazel -nt "$executable" ||
        stock_alert_nlp.py -nt "$executable" ]]; then
  needs_build=1
elif find src include tests -type f -newer "$executable" -print -quit 2>/dev/null | grep -q .; then
  needs_build=1
fi

if [[ "$needs_build" -eq 1 ]]; then
  if [[ "${1:-}" == "--rebuild" ]]; then
    ./buildCode.sh --rebuild --skip-tests
    shift
  else
    printf 'Building portfolio_health because source files changed or the executable is missing.\n'
    ./buildCode.sh --skip-tests
  fi
fi

# Capture all C++ and Python output for web sessions in a private state file.
for argument in "$@"; do
  if [[ "$argument" == "--web" ]]; then
    web_log_file="${PORTFOLIO_WEB_LOG:-${XDG_STATE_HOME:-$HOME/.local/state}/baran-capital-view/web-ui.log}"
    web_log_dir=$(dirname "$web_log_file")
    (umask 077; mkdir -p "$web_log_dir")
    if [[ -L "$web_log_file" || ( -e "$web_log_file" && ! -f "$web_log_file" ) ]]; then
      printf 'Refusing unsafe web log path: %s\n' "$web_log_file" >&2
      exit 1
    fi
    (umask 077; touch "$web_log_file")
    chmod 600 "$web_log_file"
    printf 'Capturing web logs in %s\n' "$web_log_file"
    printf 'Starting Portfolio Health Web UI at http://127.0.0.1:8080\n'
    export PYTHONUNBUFFERED=1
    exec >>"$web_log_file" 2>&1
    break
  fi
done

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
