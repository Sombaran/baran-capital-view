#!/usr/bin/env python3
"""Fetch and score recent portfolio news for Deeper analysis."""

import csv
import json
import os
import sys
import urllib.parse
import xml.etree.ElementTree as ET
from pathlib import Path

import requests

try:
    from transformers import pipeline
    from transformers import logging as tf_logging
    tf_logging.set_verbosity_error()
except Exception:
    pipeline = None

PROJECT_DIR = Path(__file__).resolve().parent
DEFAULT_CONFIG = PROJECT_DIR / "config" / "news_config.json"
POSITIVE = ("buy", "buying", "upgrade", "beat", "gain", "surge", "rally", "profit", "strong", "positive", "outperform")
NEGATIVE = ("sell", "sold", "downgrade", "miss", "loss", "drop", "decline", "fall", "fraud", "investigation", "weak", "negative", "underperform")


def load_config():
    config_path = DEFAULT_CONFIG
    if not config_path.exists():
        raise FileNotFoundError(f"Missing config: {config_path}")
    with config_path.open() as stream:
        return json.load(stream)


def safe_json_response(response):
    try:
        return response.json()
    except ValueError:
        return None


def api_news(query, config):
    headers = {"User-Agent": "myfolio-stock-alert/1.11"}
    for entry in config.get("api_keys", [])[:3]:
        endpoint = entry.get("endpoint")
        if not endpoint:
            continue
        key = os.environ.get(entry.get("env", ""), "")
        if not key:
            continue
        params = {"q": query, entry.get("param", "apiKey"): key}
        if "newsapi.org" in endpoint:
            params.update(pageSize=20, sortBy="publishedAt")
        elif "gnews.io" in endpoint:
            params.update(lang="en", max=10)
        request_headers = {**headers, **entry.get("headers", {})}
        try:
            response = requests.get(endpoint, params=params, headers=request_headers, timeout=10)
            data = safe_json_response(response)
        except (requests.RequestException, ValueError):
            continue
        if response.ok and isinstance(data, dict) and data.get("articles"):
            return data
    return None


def rss_news(query):
    try:
        response = requests.get(
            "https://news.google.com/rss/search",
            params={"q": query, "hl": "en-IN", "gl": "IN", "ceid": "IN:en"},
            headers={"User-Agent": "myfolio-stock-alert/1.11"}, timeout=10)
        response.raise_for_status()
        root = ET.fromstring(response.content)
    except (requests.RequestException, ET.ParseError):
        return []
    return [{"title": item.findtext("title", ""),
             "description": item.findtext("description", ""),
             "url": item.findtext("link", "")} for item in root.findall("./channel/item")[:10]]


def recommendation(texts, model=None):
    if model:
        scores = []
        for text in texts:
            try:
                result = model(text[:512])[0]
                score = result["score"]
                scores.append(-score if result["label"] == "NEGATIVE" else score)
            except Exception:
                pass
        if scores:
            average = sum(scores) / len(scores)
            if average > .55: return "invest more"
            if average > .15: return "going good"
            if average < -.2: return "sell it off"
            return "Is ok to hold"
    positive = sum(word in text.lower() for text in texts for word in POSITIVE)
    negative = sum(word in text.lower() for text in texts for word in NEGATIVE)
    if not positive and not negative: return "Neutral news" if texts else "No recent news"
    if positive > negative * 2: return "invest more"
    if positive >= negative: return "going good"
    return "sell it off"


def resolve_input_path(raw_path):
    path = Path(raw_path).expanduser()
    candidates = [path]
    if not path.is_absolute():
        candidates.append(PROJECT_DIR / path)
        candidates.append(Path.cwd() / path)
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return path.resolve() if path.is_absolute() else (PROJECT_DIR / path).resolve()


def main():
    if len(sys.argv) < 3:
        print("Usage: stock_alert_nlp.py stocks.csv output.csv")
        return 2
    input_path, output_path = map(Path, sys.argv[1:3])
    input_path = resolve_input_path(input_path)
    output_path = output_path if output_path.is_absolute() else (PROJECT_DIR / output_path).resolve()
    if not input_path.exists():
        print(f"Input file not found: {input_path}", file=sys.stderr)
        return 2
    config = load_config()
    suffix = config.get("query_suffix", "").strip()
    suffix = f" ({suffix})" if suffix else ""
    fast = "--fast" in sys.argv
    model = None
    if pipeline and not fast:
        try:
            model = pipeline("sentiment-analysis", model="distilbert/distilbert-base-uncased-finetuned-sst-2-english")
        except Exception:
            pass
    with input_path.open(newline="") as stream:
        rows = [(row[0].strip(), row[1].strip() if len(row) > 1 else "")
                for row in csv.reader(stream) if row and row[0].strip().lower() != "symbol"]
    json_mode = "--json" in sys.argv
    verbose = "--verbose" in sys.argv
    results = []
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as stream:
        writer = None if json_mode else csv.writer(stream)
        if writer: writer.writerow(["symbol", "name", "recommendation"])
        for symbol, name in rows:
            identity = name or symbol
            query = f"{symbol} India stock{suffix}"
            data = api_news(query, config)
            articles = data.get("articles", []) if data else []
            if not articles:
                articles = rss_news(query)
            texts = [". ".join(str(article.get(key, "")) for key in ("title", "description") if article.get(key)) for article in articles]
            result = recommendation(texts, model)
            if verbose: print(f"DEBUG:{symbol} articles={len(articles)}")
            if json_mode:
                results.append({"symbol": symbol, "name": name, "recommendation": result, "articles": articles})
            else:
                writer.writerow([symbol, name, result])
            print(symbol, identity, result)
    if json_mode:
        output_path.write_text(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
