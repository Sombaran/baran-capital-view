# Upstox Token Setup

The Upstox API key, API secret, and access token are credentials. This project
reads them from `UPSTOX_API_KEY`, `UPSTOX_API_SECRET`, and
`UPSTOX_ACCESS_TOKEN`; they are not stored in `config/config.json`.

## Quick Start

```bash
export UPSTOX_API_KEY='your-api-key'
export UPSTOX_API_SECRET='your-api-secret'
export UPSTOX_ACCESS_TOKEN='eyJ...'
./load_config.sh
./run.sh --web
```

Do not edit `run.sh` with credentials. Export the values in the current shell
or source a `chmod 600` `~/.upstox.env` file. `load_config.sh` validates the
access token without printing it. Credentials are sent only from the server;
the browser never receives them.

## Configuration File

`config/config.json` may contain non-secret endpoint settings only:

```json
{
  "upstox": {
    "base_url": "https://api.upstox.com"
  }
}
```

Do not put `access_token`, API secrets, or other credentials in this file.

## Overnight Operation

Upstox access tokens expire, commonly at the end of the trading session or
overnight. The web server keeps the most recent successful snapshot and, when
a news refresh fails, serves filtered `config/portfolio_news.json` until a
fresh request succeeds. This keeps the News page populated, but the fallback
is historical context and not a live feed. Export a newly issued token before
the next live session.

## Automation

Inject the variable through the service manager or deployment secret store:

```bash
UPSTOX_ACCESS_TOKEN="$UPSTOX_ACCESS_TOKEN" ./run.sh --web
```

Never pass the token as a command-line argument, commit it, or log it.

The client accepts only the Upstox production and sandbox HTTPS hosts for
`--base`, and authenticated curl verbose tracing is disabled. Rotate any
credentials that were previously present in repository files or logs.
