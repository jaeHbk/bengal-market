#!/usr/bin/env python3

import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile


def run(*arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        arguments,
        check=True,
        text=True,
        capture_output=True,
    )


def main() -> int:
    binary = pathlib.Path(sys.argv[1]).resolve()
    version_output = run(str(binary), "--version").stdout.strip()
    version_prefix = "bengal-market "
    assert version_output.startswith(version_prefix)
    expected_version = version_output[len(version_prefix) :]
    assert expected_version
    with tempfile.TemporaryDirectory(prefix="bengal-market-benchmark-") as root:
        directory = pathlib.Path(root)
        fixture = directory / "fixture.jsonl"
        evidence = directory / "evidence"
        run(
            str(binary),
            "generate",
            "--output",
            str(fixture),
            "--events",
            "2000",
        )
        result = run(
            str(binary),
            "benchmark",
            "--input",
            str(fixture),
            "--output",
            str(evidence),
            "--runs",
            "2",
            "--warmup",
            "1",
        )
        status = json.loads(result.stdout)
        assert status["comparable"] is True
        assert status["measured_processes"] == 4

        manifest = json.loads((evidence / "manifest.json").read_text())
        assert manifest["schema_version"] == 1
        assert manifest["comparable"] is True
        assert manifest["benchmark"]["fresh_process_per_replay"] is True
        assert manifest["benchmark"]["engine_order"] == "alternating"
        assert manifest["tool"]["version"] == expected_version
        assert len(manifest["tool"]["executable_sha256"]) == 64
        assert len(manifest["runs"]) == 4
        assert [
            (item["iteration"], item["order"], item["engine"])
            for item in manifest["runs"]
        ] == [
            (1, 1, "bengal"),
            (1, 2, "standard"),
            (2, 1, "standard"),
            (2, 2, "bengal"),
        ]
        digest = hashlib.sha256(fixture.read_bytes()).hexdigest()
        assert manifest["fixture"]["sha256"] == digest
        assert (evidence / "report.html").is_file()
        for item in manifest["runs"]:
            report = json.loads((evidence / item["report"]).read_text())
            assert report["engine"] == item["engine"]
            assert report["input"]["parse_errors"] == 0
            assert report["pipeline"]["dropped"] == 0
            assert (evidence / item["stderr"]).read_text() == ""

        duplicate = subprocess.run(
            [
                str(binary),
                "benchmark",
                "--input",
                str(fixture),
                "--output",
                str(evidence),
                "--runs",
                "1",
            ],
            text=True,
            capture_output=True,
        )
        assert duplicate.returncode == 2
        assert "already exists" in duplicate.stderr

        malformed = directory / "malformed.jsonl"
        malformed.write_bytes(
            fixture.read_bytes()
            + b'{"type":"frame","received_ns":"invalid"}\n'
        )
        invalid_evidence = directory / "invalid-evidence"
        invalid = subprocess.run(
            [
                str(binary),
                "benchmark",
                "--input",
                str(malformed),
                "--output",
                str(invalid_evidence),
                "--runs",
                "1",
                "--warmup",
                "0",
            ],
            text=True,
            capture_output=True,
        )
        assert invalid.returncode == 1
        assert json.loads(invalid.stdout)["comparable"] is False
        invalid_manifest = json.loads(
            (invalid_evidence / "manifest.json").read_text()
        )
        assert invalid_manifest["comparable"] is False
        assert len(invalid_manifest["runs"]) == 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
