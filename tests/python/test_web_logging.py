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
    executable = build / "portfolio_health"
    executable.write_text(
        "#!/bin/sh\nprintf 'cpp stdout marker\\n'\nprintf 'cpp stderr marker\\n' >&2\n"
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
