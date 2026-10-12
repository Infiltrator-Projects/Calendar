#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Static contracts for Calendar's externally observable release model.

This suite deliberately checks packaging, publication, repository structure,
versioning, licensing and immutable release inputs. Behavioural and internal
architecture contracts belong to the C/JavaScript/architecture tests instead of
being frozen here as exact source spelling.
"""

from __future__ import annotations

import json
import re
import stat
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> None:
    makefile = read("Makefile")
    rules = read("debian/rules")
    installer = read("tools/local-installer.sh.in")
    builder = read("tools/build-local-installer.sh")
    release = read("tools/release-check.sh")
    workflow = read(".github/workflows/ci.yml")
    publisher = read(".github/workflows/release.yml")
    artifact_validator = read("tools/validate-release-artifacts.sh")
    native_installer_smoke = read("tools/native-installer-smoke.sh")
    gitmodules = read(".gitmodules")
    copyright_file = read("debian/copyright")
    control = read("debian/control")

    assert sorted(path.name for path in ROOT.glob("*.md")) == [
        "CHANGELOG.md", "CONTRIBUTING.md", "README.md", "SECURITY.md"
    ]
    assert sorted(path.name for path in (ROOT / ".github").glob("*.md")) == [
        "CODE_OF_CONDUCT.md"
    ]
    assert sorted(path.name for path in (ROOT / "docs").glob("*.md")) == [
        "ARCHITECTURE.md", "BRANCH-RETIREMENT-2026-10-12.md", "DECISIONS.md", "DESIGN.md",
        "MODELS.md", "PORTABILITY.md", "QUALIFICATION.md", "README.md", "ROADMAP.md", "VALIDATION.md"
    ]
    assert (ROOT / ".github/ISSUE_TEMPLATE/bug_report.yml").is_file()
    assert (ROOT / ".github/ISSUE_TEMPLATE/feature_request.yml").is_file()
    assert "## Range and continuation matrix" in read("docs/MODELS.md")
    assert "## Current development boundary" in read("docs/QUALIFICATION.md")
    assert (ROOT / "LICENSE").is_file()
    assert not (ROOT / "COPYING").exists()
    assert "DEVELOPING.md" not in makefile
    assert "install -m644 README.md" in makefile

    # Scripts invoked directly by Make, CI or another executable script must
    # retain their Git executable bit. A lost mode is a release-breaking source
    # defect even when the script contents themselves are unchanged.
    for executable in (
        "tools/build-local-installer.sh",
        "tools/cinnamon-smoke.sh",
        "tools/live-cinnamon-ci-smoke.sh",
        "tools/native-installer-smoke.sh",
        "tools/path-space-build.sh",
        "tools/release-check.sh",
        "tools/reproducible-build.sh",
        "tools/validate-release-artifacts.sh",
    ):
        assert (ROOT / executable).stat().st_mode & stat.S_IXUSR, (
            f"{executable} must be executable"
        )

    # Build-profile and source-archive policy are release identities, not
    # implementation details. Keep these explicit because produced artifacts
    # must remain reproducible and distinguish generic from native/PGO builds.
    assert "BUILD_MODE ?= generic" in makefile
    assert "-O2 -g" in makefile
    assert "-O3 -g -march=native -mtune=native -flto=auto" in makefile
    assert "PGO_MODE ?= none" in makefile
    assert "-fprofile-generate=$(PGO_DIR)" in makefile
    assert "-fprofile-use=$(PGO_DIR)" in makefile
    assert "-fprofile-correction" in makefile
    assert "-fprofile-partial-training" in makefile
    assert "-Wno-error=missing-profile" in makefile
