#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Contract tests for the Cinnamon upstream-drift monitor.

The scheduled monitor is a release-maintenance safety boundary: a negative
result is meaningful only when GitHub returned a complete, structurally valid
comparison. These tests protect that fail-closed property without depending on
live network state or the current upstream Cinnamon revision.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from types import ModuleType
from typing import Callable


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools/check-upstream-calendar-drift.py"

# Import the monitor as code under test without creating a release-input cache.
sys.dont_write_bytecode = True


def load_tool() -> ModuleType:
    spec = importlib.util.spec_from_file_location("calendar_upstream_drift", TOOL)
    if spec is None or spec.loader is None:
        raise AssertionError("cannot load upstream-drift monitor")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def assert_runtime_error(call: Callable[[], object], fragment: str) -> None:
    try:
        call()
    except RuntimeError as error:
        assert fragment in str(error), str(error)
    else:
        raise AssertionError(f"expected RuntimeError containing {fragment!r}")


def main() -> None:
    tool = load_tool()

    assert tool.branch_head_sha({"commit": {"sha": "abc123"}}) == "abc123"
    assert_runtime_error(lambda: tool.branch_head_sha([]), "root")
    assert_runtime_error(lambda: tool.branch_head_sha({}), "commit")
    assert_runtime_error(lambda: tool.branch_head_sha({"commit": {}}), "SHA")

    empty = tool.complete_compare_files({"files": []})
    assert empty == []

    complete = [
        {"filename": f"docs/unrelated-{index}.md"}
        for index in range(tool.GITHUB_COMPARE_FILE_LIMIT - 1)
    ]
    assert tool.complete_compare_files({"files": complete}) == complete

    truncated = complete + [{"filename": "files/usr/share/cinnamon/applets/calendar.js"}]
    assert len(truncated) == tool.GITHUB_COMPARE_FILE_LIMIT
    assert_runtime_error(
        lambda: tool.complete_compare_files({"files": truncated}),
        "300-file API limit",
    )
    assert_runtime_error(
        lambda: tool.complete_compare_files({"files": "not-a-list"}),
        "changed-file list",
    )
    assert_runtime_error(
        lambda: tool.complete_compare_files({"files": [{}]}),
        "invalid file entry",
    )
    assert_runtime_error(lambda: tool.complete_compare_files([]), "root")

    print("Upstream drift monitor contract tests passed.")


if __name__ == "__main__":
    main()
