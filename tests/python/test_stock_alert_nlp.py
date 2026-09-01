import csv
import json
from pathlib import Path

import pytest

import stock_alert_nlp


@pytest.fixture
def sample_rows(tmp_path):
    input_path = tmp_path / "stocks.csv"
    input_path.write_text("symbol,name\nRELIANCE,Reliance Industries\nTCS,Tata Consultancy Services\n")
    return input_path


def test_load_config_reads_expected_values():
    config = stock_alert_nlp.load_config()
    assert isinstance(config, dict)
    assert "api_keys" in config


def test_recommendation_uses_sentiment_logic():
    assert stock_alert_nlp.recommendation(["profit rises strongly", "growth beats estimates"]) == "invest more"
    assert stock_alert_nlp.recommendation(["drop in margins and losses"]) == "sell it off"
    assert stock_alert_nlp.recommendation(["market update", "company note"]) == "Neutral news"


def test_recommendation_uses_model_scores_when_available():
    class FakeModel:
        def __call__(self, text):
            return [{"label": "POSITIVE", "score": 0.90}]

    assert stock_alert_nlp.recommendation(["the company reports stable results"], FakeModel()) == "invest more"


def test_resolve_input_path_prefers_existing_project_file():
    project_dir = Path(stock_alert_nlp.PROJECT_DIR)
    candidate = project_dir / "config" / "holding.csv"
    resolved = stock_alert_nlp.resolve_input_path("config/holding.csv")
    assert resolved == candidate.resolve()


def test_safe_json_response_handles_invalid_payload():
    class FakeResponse:
        def json(self):
            raise ValueError("bad json")

    assert stock_alert_nlp.safe_json_response(FakeResponse()) is None


def test_api_news_returns_first_valid_provider_payload(monkeypatch):
    class FakeResponse:
        def __init__(self, payload):
            self._payload = payload
            self.ok = True

        def json(self):
            return self._payload

    def fake_get(url, params=None, headers=None, timeout=None):
        return FakeResponse({"articles": [{"title": "India growth story", "description": "profit rises"}]})

    monkeypatch.setattr(stock_alert_nlp.requests, "get", fake_get)
    monkeypatch.setenv("NEWSAPI_KEY", "demo-key")

    result = stock_alert_nlp.api_news("RELIANCE India stock", stock_alert_nlp.load_config())
    assert result is not None
    assert result["articles"][0]["title"] == "India growth story"


def test_rss_news_handles_parse_error(monkeypatch):
    def fake_get(url, params=None, headers=None, timeout=None):
        raise stock_alert_nlp.requests.RequestException("no network")

    monkeypatch.setattr(stock_alert_nlp.requests, "get", fake_get)
    assert stock_alert_nlp.rss_news("RELIANCE") == []


def test_main_writes_csv_output(sample_rows, tmp_path, monkeypatch):
    output = tmp_path / "output.csv"
    import sys

    monkeypatch.setattr(stock_alert_nlp, "api_news", lambda query, config: {"articles": []})
    monkeypatch.setattr(stock_alert_nlp, "rss_news", lambda query: [])

    original_argv = sys.argv[:]
    sys.argv = [
        "stock_alert_nlp.py",
        str(sample_rows),
        str(output),
        "--fast",
    ]
    try:
        exit_code = stock_alert_nlp.main()
    finally:
        sys.argv = original_argv

    assert exit_code == 0
    assert output.exists()
    with output.open(newline="") as handle:
        rows = list(csv.reader(handle))
    assert rows[0] == ["symbol", "name", "recommendation"]
    assert len(rows) >= 2
    assert rows[1][2] in {"No recent news", "Neutral news"}
