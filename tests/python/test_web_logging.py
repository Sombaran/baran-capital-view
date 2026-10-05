import os
import shutil
import stat
import subprocess
from pathlib import Path


def prepare_fake_project(tmp_path):
    project = tmp_path / "project"
    build = project / "build"
    build.mkdir(parents=True)
    shutil.copy2(Path(__file__).parents[2] / "run.sh", project / "run.sh")
    build_script = project / "buildCode.sh"
    build_script.write_text(
        "#!/bin/sh\nprintf 'build marker: %s\\n' \"$*\"\n"
    )
    build_script.chmod(0o755)
    executable = build / "portfolio_health"
    executable.write_text(
        "#!/bin/sh\nprintf 'cpp stdout marker\\n'\nprintf 'cpp stderr marker\\n' >&2\nprintf 'token marker: %s\\n' \"${UPSTOX_ACCESS_TOKEN:-unset}\"\n"
    )
    executable.chmod(0o755)
    home = tmp_path / "home"
    home.mkdir()
    (home / ".upstox.env").write_text("")
    return project, home


def test_web_launch_captures_cpp_output_in_private_log(tmp_path):
    project, home = prepare_fake_project(tmp_path)
    state_home = tmp_path / "state"
    environment = os.environ.copy()
    environment.update({"HOME": str(home), "XDG_STATE_HOME": str(state_home)})

    result = subprocess.run(
        ["bash", str(project / "run.sh"), "--web"],
        cwd=project,
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )

    log_file = state_home / "baran-capital-view" / "web-ui.log"
    contents = log_file.read_text()
    assert "cpp stdout marker" in contents
    assert "cpp stderr marker" in contents
    assert "token marker: unset" in contents
    assert stat.S_IMODE(log_file.stat().st_mode) == 0o600
    assert str(log_file) in result.stdout
    assert "http://127.0.0.1:8080" in result.stdout
    assert "cpp stdout marker" not in result.stdout


def test_non_web_launch_is_not_redirected(tmp_path):
    project, home = prepare_fake_project(tmp_path)
    state_home = tmp_path / "state"
    environment = os.environ.copy()
    environment.update({"HOME": str(home), "XDG_STATE_HOME": str(state_home)})

    result = subprocess.run(
        ["bash", str(project / "run.sh"), "--version"],
        cwd=project,
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )

    assert "cpp stdout marker" in result.stdout
    assert "cpp stderr marker" in result.stderr
    assert not (state_home / "baran-capital-view" / "web-ui.log").exists()


def test_fresh_shell_token_overrides_stale_env_file(tmp_path):
    project, home = prepare_fake_project(tmp_path)
    (home / ".upstox.env").write_text(
        "export UPSTOX_API_KEY=file-key\n"
        "export UPSTOX_API_SECRET=file-secret\n"
        "export UPSTOX_ACCESS_TOKEN=stale-file-token\n"
    )
    state_home = tmp_path / "state"
    environment = os.environ.copy()
    environment.update({
        "HOME": str(home),
        "XDG_STATE_HOME": str(state_home),
        "UPSTOX_ACCESS_TOKEN": "fresh-shell-token",
    })

    subprocess.run(
        ["bash", str(project / "run.sh"), "--web"],
        cwd=project,
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )

    log_file = state_home / "baran-capital-view" / "web-ui.log"
    contents = log_file.read_text()
    assert "token marker: fresh-shell-token" in contents
    assert "stale-file-token" not in contents


def test_launcher_rebuilds_when_source_is_newer_than_the_binary(tmp_path):
    project, home = prepare_fake_project(tmp_path)
    source = project / "src"
    source.mkdir()
    source_file = source / "WebServer.cpp"
    source_file.write_text("// updated source\n")
    executable = project / "build" / "portfolio_health"
    source_time = executable.stat().st_mtime_ns + 1_000_000_000
    os.utime(source_file, ns=(source_time, source_time))
    environment = os.environ.copy()
    environment.update({"HOME": str(home)})

    result = subprocess.run(
        ["bash", str(project / "run.sh"), "--version"],
        cwd=project,
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )

    assert "build marker: --skip-tests" in result.stdout
    assert "cpp stdout marker" in result.stdout


def test_launcher_rebuild_option_is_consumed_before_forwarding_arguments(tmp_path):
    project, home = prepare_fake_project(tmp_path)
    environment = os.environ.copy()
    environment.update({"HOME": str(home)})

    result = subprocess.run(
        ["bash", str(project / "run.sh"), "--rebuild", "--version"],
        cwd=project,
        env=environment,
        capture_output=True,
        text=True,
        check=True,
    )

    assert "build marker: --rebuild --skip-tests" in result.stdout
    assert "cpp stdout marker" in result.stdout


def test_operations_workspace_keeps_existing_subviews():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    operations = source.split("const char* operationsWorkspace()", 1)[1].split(
        "const char* responsiveDashboardNavigation()", 1
    )[0]

    assert "operationsButton.dataset.tab='operations'" in operations
    assert 'data-operation="positions"' in operations
    assert 'data-operation="health"' in operations
    assert 'data-operation="config"' in operations
    assert "raw('positions','/api/positions','Open positions')" in operations
    assert "await health()" in operations
    assert "await config()" in operations


def test_operations_download_uses_authenticated_holdings_and_safe_csv():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    operations = source.split("const char* operationsWorkspace()", 1)[1].split(
        "const char* operationsDownload()", 1
    )[0]
    exporter = source.split("const char* operationsDownload()", 1)[1].split(
        "const char* responsiveDashboardNavigation()", 1
    )[0]

    assert 'data-operation="download"' in operations
    assert "data-download-portfolio" in operations
    assert "get('holdings','/api/holdings')" in exporter
    assert "text.replace(/\"/g,'\"\"')" in exporter
    assert "typeof value==='string'&&!numeric" in exporter
    assert "URL.revokeObjectURL(url)" in exporter


def test_market_news_uses_one_tab_and_keeps_legacy_route():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    consolidation = source.split("const char* consolidatedMarketNewsTab()", 1)[1].split(
        "const char* requestedDashboardReleaseNotice()", 1
    )[0]
    renderer = source.split("async function news()", 1)[1].split(
        "\nasync function alerts()", 1
    )[0]

    assert "nav.querySelector('[data-tab=\"global-news\"]')?.remove()" in consolidation
    assert "newsTab.textContent='Market news'" in consolidation
    assert "get('news','/api/news')" in renderer
    assert "path == \"/api/global-news\"" in source
    assert "client_.getNews(\"holdings\")" in source
    assert "releaseNoticeV2043Addendum()" in source
    assert "dashboardPageOptimization()" in source


def test_dashboard_snapshot_retries_are_bounded_and_failures_are_isolated():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    snapshot = source.split(
        "std::shared_ptr<const WebServer::Snapshot> WebServer::snapshot(bool forceRefresh) const",
        1,
    )[1].split("\nint WebServer::run()", 1)[0]
    overview = source.split("async function holdings()", 1)[1].split(
        "\nasync function news()", 1
    )[0]
    market_news = source.split("async function news()", 1)[1].split(
        "\nasync function alerts()", 1
    )[0]

    assert "std::chrono::seconds(30)" in snapshot
    assert "snapshotRetryAfter_" in snapshot
    assert "std::chrono::minutes(5)" in snapshot
    assert '"upstox-stale"' in snapshot
    assert '"snapshot_age_seconds"' in snapshot
    assert snapshot.index("fresh->created =") > snapshot.index('client_.getNews("holdings")')
    assert "last successful news snapshot" in snapshot
    assert "Portfolio news is temporarily unavailable." in snapshot
    assert "catch(error){newsUnavailable=true}" in overview
    assert "portfolio holdings are still shown" in overview
    assert "Cached news · live refresh unavailable" in market_news
    assert "releaseNoticeV2044Addendum()" in source


def test_snapshot_failure_keeps_last_good_data_and_resolves_fallback_from_project():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    snapshot = source.split(
        "std::shared_ptr<const WebServer::Snapshot> WebServer::snapshot(bool forceRefresh) const",
        1,
    )[1].split("\nint WebServer::run()", 1)[0]
    local_data = source.split("std::filesystem::path projectFile(", 1)[1].split(
        "\nstd::string localNews", 1
    )[0]
    constructor = source.split("WebServer::WebServer(", 1)[1].split(
        "\nstd::string WebServer::fundamentalsAnalysis", 1
    )[0]

    assert snapshot.index("if (current) return staleSnapshot(current)") < snapshot.index(
        "const json fallback = localHoldings(holdingsFile_)"
    )
    assert "std::filesystem::path projectRoot()" in source
    assert "projectRoot() / configured" in local_data
    assert "holdingsFile_(projectFile(holdingsFile).string())" in constructor
    assert "UPSTOX_ACCESS_TOKEN and restart the dashboard" in snapshot
    assert 'releaseNoticeV2048Addendum()' in source


def test_web_server_handles_slow_snapshots_concurrently_with_a_connection_limit():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    snapshot = source.split(
        "std::shared_ptr<const WebServer::Snapshot> WebServer::snapshot(bool forceRefresh) const",
        1,
    )[1].split("\nint WebServer::run()", 1)[0]
    server = source.split("int WebServer::run()", 1)[1]

    assert "std::unique_lock<std::mutex> refreshLock(refreshMutex_, std::try_to_lock)" in snapshot
    assert "if (!refreshLock.owns_lock())" in snapshot
    assert "if (current) return staleSnapshot(current)" in snapshot
    assert "maxConcurrentConnections = 8" in server
    assert "std::thread([this, connection, &activeConnections]" in server
    assert '"503 Service Unavailable"' in server
    assert "activeConnections.fetch_sub(1, std::memory_order_acq_rel)" in server
    assert "releaseNoticeV2049Addendum()" in server


def test_background_refresh_failure_retains_last_rendered_dashboard():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    failure = source.split("function fail(e)", 1)[1].split("\nfunction rows", 1)[0]

    assert "hasRenderedView=view.childElementCount>0&&!view.querySelector('.loading,[role=\"alert\"]')" in failure
    assert "the last displayed data is retained. Use Refresh to retry." in failure
    assert "view.prepend(warning)" in failure
    assert "view.innerHTML='<section class=\"panel error\"" in failure
    assert "Live refresh unavailable · current view retained" in failure


def test_manual_refresh_bypasses_server_backoff_with_a_rate_limit():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    header = (Path(__file__).parents[2] / "include" / "WebServer.hpp").read_text()
    refresh_ui = source.split("function refreshView()", 1)[1].split(
        "const baseSafeJsonFetch", 1
    )[0]
    fetcher = source.split("async function get(name,url)", 1)[1].split(
        "\nasync function safeJsonParse", 1
    )[0]
    snapshot = source.split(
        "std::shared_ptr<const WebServer::Snapshot> WebServer::snapshot(bool forceRefresh) const",
        1,
    )[1].split("\nint WebServer::run()", 1)[0]

    assert "manualRefreshRequested=true" in refresh_ui
    assert "button.disabled=true" in refresh_ui
    assert "button.textContent='Refreshing...'" in refresh_ui
    assert "button.disabled=false" in refresh_ui
    assert "manualRefreshRequested&&['/api/holdings','/api/positions','/api/news'].includes(url)" in fetcher
    assert "requestUrl=url+'?refresh=1'" in fetcher
    assert "snapshot(bool forceRefresh = false)" in header
    assert "const bool manualRefreshQuery = queryStart != std::string::npos" in source
    assert "path == \"/api/holdings\" || path == \"/api/positions\" || path == \"/api/news\"" in source
    assert "snapshot(forceRefresh)" in source
    assert source.index("else if (!authenticated(requestText))") < source.index("snapshot(forceRefresh)")
    assert "manualRefreshLimit = std::chrono::seconds(10)" in snapshot
    assert "if (!forceRefresh && checkedAt < snapshotRetryAfter_)" in snapshot
    assert "if (checkedAt < manualRefreshAfter_)" in snapshot
    assert "releaseNoticeV2046Addendum()" in source


def test_query_routes_preserve_parameters_and_fundamentals_popup_wraps():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    routing = source.split("int WebServer::run()", 1)[1]
    fundamental_ui = source.split("function addFundamentalSummary()", 1)[1].split(
        "\nfunction fundamentalValue", 1
    )[0]
    popup_styles = source.split("id=\"fundamental-popup-style\"", 1)[1].split(
        "</style>", 1
    )[0]

    assert "if (forceRefresh) path.resize(queryStart)" in routing
    assert "if (queryStart != std::string::npos) path.resize(queryStart)" not in routing
    assert 'path.rfind("/api/fundamentals?symbol=", 0) == 0' in routing
    assert 'path.rfind("/api/stock-analysis?symbol=", 0) == 0' in routing
    assert 'path.rfind("/api/market-quotes?instrument_key=", 0) == 0' in routing
    assert 'path.rfind("/api/rsi?closes=", 0) == 0' in routing
    assert 'path.rfind("/api/market-quote/ohlc?instrument_key=", 0) == 0' in routing
    assert "table-layout:fixed" in popup_styles
    assert "overflow-wrap:anywhere" in popup_styles
    assert "releaseNoticeV2047Addendum()" in source


def test_deeper_analysis_deduplicates_symbols_and_keeps_ui_categories_consistent():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    analysis = source.split("std::string WebServer::runDeeperAnalysis() const", 1)[1].split(
        "\nbool WebServer::authenticated", 1
    )[0]
    renderer = source.split("const char* dashboardUiOptimization()", 1)[1].split(
        "\nconst char* dashboardResponsiveLayout()", 1
    )[0]
    page_composition = source.split("html.replace(html.find(marker)", 1)[1].split(
        "const std::string head", 1
    )[0]

    assert "uniqueAnalysisSymbols(liveSymbols)" in analysis
    assert 'outputPath + "\\" --json"' in analysis
    assert "json::parse(resultText.str())" in analysis
    assert 'recommendations.emplace(symbol, recommendation)' in analysis
    assert '\"category\", category' in analysis
    assert "categoryStocks[category].push_back(symbol)" in analysis
    assert "marketValue += holding->marketValue()" in analysis
    assert "stocks.forEach(item=>{const label=order.includes(item.category)" in renderer
    assert "counts[label]++" in renderer
    assert "window.deeperCategories={counts,stocks:categoryStocks}" in renderer
    assert "<th>Category</th>" in renderer
    assert "names=categoryStocks[label]" in renderer
    assert "categoryEnhancements()" not in page_composition
    assert "releaseNoticeV2045Addendum()" in source


def test_operations_download_uses_safe_csv_from_authenticated_holdings():
    source = (Path(__file__).parents[2] / "src" / "WebServer.cpp").read_text()
    operations = source.split("const char* operationsWorkspace()", 1)[1].split(
        "const char* operationsDownload()", 1
    )[0]
    exporter = source.split("const char* operationsDownload()", 1)[1].split(
        "const char* responsiveDashboardNavigation()", 1
    )[0]

    assert 'data-operation="download"' in operations
    assert "data-download-portfolio" in operations
    assert "get('holdings','/api/holdings')" in exporter
    assert "text.replace(/\"/g,'\"\"')" in exporter
    assert "formula" in exporter or "numeric" in exporter
    assert "URL.revokeObjectURL(url)" in exporter
