#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Report relevant Cinnamon calendar changes after the reviewed baseline.

This is deliberately a monitoring tool, not a source dependency. Calendar
never downloads or incorporates Cinnamon code during its ordinary build. A
scheduled CI job compares upstream history and fails only when files in the
reviewed calendar integration surface changed, forcing a human compatibility
review before the baseline is advanced.

GitHub's compare API exposes at most 300 changed-file entries for a comparison.
A baseline can legitimately age through more unrelated Cinnamon changes than
that, so reaching the limit is not evidence that watched files were unchanged.
The monitor therefore fails closed at that boundary rather than certifying an
incomplete comparison.
"""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "tools/upstream-calendar-baseline.json"
GITHUB_COMPARE_FILE_LIMIT = 300


def api_json(url: str) -> object:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "calendar-plus-upstream-drift",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except (urllib.error.HTTPError, urllib.error.URLError) as error:
        raise RuntimeError(f"GitHub API request failed: {error}") from error


def branch_head_sha(response: object) -> str:
    """Return the branch head SHA only from the expected GitHub response shape."""
    if not isinstance(response, dict):
        raise RuntimeError("GitHub branch response root is not an object")
    commit = response.get("commit")
    if not isinstance(commit, dict):
        raise RuntimeError("GitHub branch response has no commit object")
    sha = commit.get("sha")
    if not isinstance(sha, str) or not sha:
        raise RuntimeError("GitHub branch response has no commit SHA")
    return sha


def complete_compare_files(response: object) -> list[dict[str, object]]:
    """Return a trustworthy changed-file list or fail before drift is certified.

    GitHub caps the changed-file portion of a compare response at 300 entries.
    There is no safe ordering assumption that lets Calendar infer whether a
    watched path would have appeared after that cap, so a full list is a hard
    precondition for the negative conclusion "no watched calendar changes".
    """
    if not isinstance(response, dict):
        raise RuntimeError("GitHub compare response root is not an object")
    files = response.get("files")
    if not isinstance(files, list):
        raise RuntimeError("GitHub compare response has no changed-file list")
    if len(files) >= GITHUB_COMPARE_FILE_LIMIT:
        raise RuntimeError(
            "GitHub compare changed-file list reached its 300-file API limit; "
            "the comparison cannot safely certify absence of watched Cinnamon "
            "drift. Review upstream and advance the baseline before retrying."
        )

    validated: list[dict[str, object]] = []
    for entry in files:
        if not isinstance(entry, dict) or not isinstance(entry.get("filename"), str):
            raise RuntimeError("GitHub compare response contains an invalid file entry")
        validated.append(entry)
    return validated


def main() -> None:
    config = json.loads(BASELINE.read_text(encoding="utf-8"))
    repository = config["repository"]
    reference = config["ref"]
    baseline = config["commit"]
    watched = tuple(config["paths"])

    try:
        branch_url = (
            f"https://api.github.com/repos/{repository}/branches/"
            f"{urllib.parse.quote(reference, safe='')}"
        )
        latest = branch_head_sha(api_json(branch_url))
        if latest == baseline:
            print(f"Cinnamon calendar baseline is current at {baseline}.")
            return

        compare_url = (
            f"https://api.github.com/repos/{repository}/compare/"
            f"{baseline}...{latest}?per_page=100"
        )
        files = complete_compare_files(api_json(compare_url))
    except RuntimeError as error:
        print(
            f"Cannot certify Cinnamon calendar drift state: {error}",
            file=sys.stderr,
        )
        raise SystemExit(1) from error

    changed = sorted(
        str(entry["filename"])
        for entry in files
        if any(
            entry["filename"] == path or str(entry["filename"]).startswith(path)
            for path in watched
        )
    )

    if not changed:
        print(
            "Cinnamon advanced from "
            f"{baseline} to {latest}, with no watched calendar changes."
        )
        return

    print(
        "Relevant upstream Cinnamon calendar drift detected. "
        "Review these files before advancing the baseline:",
        file=sys.stderr,
    )
    for path in changed:
        print(f"  {path}", file=sys.stderr)
    print(f"Reviewed baseline: {baseline}", file=sys.stderr)
    print(f"Current upstream:  {latest}", file=sys.stderr)
    raise SystemExit(1)


if __name__ == "__main__":
    main()
