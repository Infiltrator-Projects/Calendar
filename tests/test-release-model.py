#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Behavioural contracts for Calendar's build and release model.

Keep this suite focused on externally meaningful invariants. Implementation
spelling, file counts and workflow step names are deliberately not contracts.
"""

from __future__ import annotations

import json
import re
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def make_var(makefile: str, name: str) -> str:
    match = re.search(rf"^{re.escape(name)} := (.+)$", makefile, re.MULTILINE)
    assert match, f"missing Makefile variable {name}"
    return match.group(1).strip()


def first_changelog_version(changelog: str) -> str:
    match = re.match(r"infiltrator-calendar \(([^)]+)\) ", changelog)
    assert match, "unexpected debian/changelog header"
    return match.group(1)


def assert_release_handoff_behaviour(version: str) -> None:
    tool = ROOT / "tools/release-handoff.py"
    assert tool.is_file()

    with tempfile.TemporaryDirectory(prefix="calendar-release-model-") as tmp:
        dist = Path(tmp)
        deb = dist / f"calendar_{version}_amd64.deb"
        installer = dist / f"calendar-{version}-local-folder.run"
        deb.write_bytes(b"calendar-deb-test\n")
        installer.write_bytes(b"calendar-installer-test\n")
        source_sha = "a" * 40

        subprocess.run(
            [
                "python3",
                str(tool),
                "create",
                "--dist",
                str(dist),
                "--source-sha",
                source_sha,
            ],
            cwd=ROOT,
            check=True,
        )

        manifest = json.loads(
            (dist / "release-manifest.json").read_text(encoding="utf-8")
        )
        assert manifest["schema"] == 1
        assert manifest["version"] == version
        assert manifest["tag"] == f"v{version}"
        assert manifest["source_sha"] == source_sha
        assert {asset["name"] for asset in manifest["assets"]} == {
            deb.name,
            installer.name,
        }
        assert (dist / "release-notes.md").read_text(encoding="utf-8").strip()
        assert (dist / "release-handoff.py").is_file()

        subprocess.run(
            [
                "python3",
                str(tool),
                "verify",
                "--dist",
                str(dist),
                "--expected-sha",
                source_sha,
            ],
            cwd=ROOT,
            check=True,
        )

        deb.write_bytes(b"tampered\n")
        result = subprocess.run(
            [
                "python3",
                str(tool),
                "verify",
                "--dist",
                str(dist),
                "--expected-sha",
                source_sha,
            ],
            cwd=ROOT,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        assert result.returncode != 0, "tampered release asset was accepted"


def main() -> None:
    makefile = read("Makefile")
    rules = read("debian/rules")
    installer = read("tools/local-installer.sh.in")
    release_check = read("tools/release-check.sh")
    workflow = read(".github/workflows/ci.yml")
    publisher = read(".github/workflows/release.yml")
    control = read("debian/control")
    changelog = read("debian/changelog")
    metadata = json.loads(read("src/cinnamon/metadata.json"))
    copyright_file = read("debian/copyright")

    # Product identity has several consumers, but one release may never carry
    # disagreeing versions.
    version = make_var(makefile, "VERSION")
    assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version)
    assert metadata["version"] == version
    assert first_changelog_version(changelog) == version
    assert metadata["author"] == "Shannon Smith"
    assert metadata["website"] == "https://github.com/Infiltrator-Projects/Calendar"

    # Build profiles remain deliberate and package construction remains generic.
    assert "BUILD_MODE ?= generic" in makefile
    assert "PGO_MODE ?= none" in makefile
    assert "-march=native" in makefile
    assert "CALENDAR_PLUS_BUILD_MODE ?= generic" in rules
    assert "BUILD_MODE=$(CALENDAR_PLUS_BUILD_MODE)" in rules
    assert "filter nocheck,$(DEB_BUILD_OPTIONS)" in rules
    assert "export CFLAGS" not in makefile

    # Common is pinned, independently versioned and consumed through its public
    # archive/header boundary without freezing today's particular pin in this test.
    common_version = make_var(makefile, "INFILTRATR_COMMON_VERSION")
    common_commit = make_var(makefile, "INFILTRATR_COMMON_COMMIT")
    assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", common_version)
    assert re.fullmatch(r"[0-9a-f]{40}", common_commit)
    common = ROOT / "src/vendor/infiltratr-common"
    assert (common / "VERSION").read_text(encoding="utf-8").strip() == common_version
    assert (common / "LICENSE").is_file()
    assert "libinfiltratr-common.a" in makefile
    assert "src/vendor/infiltratr-common" in makefile
    assert "Infiltrator-Projects/Infiltrator-Libraries.git" in read(".gitmodules")
    for private_source in ("core.c", "arithmetic.c", "timing.c", "dynlib.c"):
        assert f"$(INFILTRATR_COMMON_DIR)/src/{private_source}" not in makefile

    # Calendar may call only APIs present in the pinned Common public headers.
    common_include = common / "include/infiltratr"
    public_api = "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted(common_include.glob("*.h"))
    )
    call_pattern = re.compile(r"\b(infiltratr_[a-z0-9_]+)\s*\(")
    calendar_common_calls: set[str] = set()
    for source_dir in ("app", "core", "adapters"):
        for path in sorted((ROOT / "src" / source_dir).glob("*.[ch]")):
            calendar_common_calls.update(
                call_pattern.findall(path.read_text(encoding="utf-8"))
            )
    assert calendar_common_calls
    missing_common_api = sorted(
        function
        for function in calendar_common_calls
        if not re.search(rf"\b{re.escape(function)}\s*\(", public_api)
    )
    assert not missing_common_api, (
        "Calendar calls APIs absent from pinned Common public headers: "
        f"{missing_common_api}"
    )

    # Proprietary font binaries are not release inputs; typography metadata may
    # still nominate the preferred family with a required fallback.
    assert "FONT_ARCHIVE" not in makefile
    assert "prepare-fonts" not in makefile
    assert not (ROOT / "src/assets/fonts/mb-corpo-fonts.tar.xz").exists()
    common_design = json.loads(
        read("src/vendor/infiltratr-common/design/infiltrator-design-v1.json")
    )
    policy = common_design["policy"]
    assert policy["font_binaries_are_not_part_of_this_contract"] is True
    assert policy["fallback_is_required_when_mb_corpo_is_unavailable"] is True

    # Debian package identity and runtime boundaries.
    assert "Source: infiltrator-calendar" in control
    assert "\nPackage: infiltrator-calendar\n" in control
    assert "\nPackage: calendar-plus\n" not in control
    assert "Breaks: calendar-plus, cinnamon-calendar" in control
    assert "Replaces: calendar-plus, cinnamon-calendar" in control
    assert "cinnamon (>= 6.4)" in control
    binary_stanza = control.split("\nPackage: infiltrator-calendar\n", 1)[1]
    runtime_control = binary_stanza.split("Description:", 1)[0]
    assert "libgtk-3-0," not in runtime_control
    assert "\n python3," not in runtime_control
    assert "-DU_DISABLE_RENAMING=1" in makefile
    assert "src/core/icu-compat-bridge.c" in makefile
    assert "validate-runtime-deps:" in makefile

    # The local installer remains a native/PGO path distinct from the generic
    # package and keeps payload integrity checks.
    assert 'NATIVE_VERSION="${VERSION}+nativepgo${NATIVE_REVISION}"' in installer
    assert "CALENDAR_PLUS_BUILD_MODE=native" in installer
    assert "PGO_MODE=generate" in installer
    assert "PGO_MODE=use" in installer
    assert "--build-only" in installer
    assert "--verify-only" in installer
    assert "@PAYLOAD_SHA256@" in installer

    # Release construction qualifies reproducibility, artifact shape and native
    # installer behaviour exactly once.
    assert "CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED" in release_check
    assert "make reproducible-build" in release_check
    assert "tools/native-installer-smoke.sh" in release_check
    assert "tools/validate-release-artifacts.sh" in release_check
    assert "lintian --fail-on error" in release_check

    # Release intent is data: a monotonic VERSION transition on main. Commit
    # prose is never part of the protocol.
    assert "release-handoff.py release-requested" in workflow
    assert "release_requested:" in workflow
    assert "needs.debian-package.outputs.release_requested == 'true'" in workflow
    assert "head_commit.message" not in workflow
    assert "startsWith(" not in workflow
    assert "cancel-in-progress: false" in workflow

    # Publication consumes one JSON handoff and never rebuilds. GitHub release
    # creation owns tag creation; Calendar does not mutate tags as a recovery
    # mechanism.
    assert "release-manifest.json" in workflow
    assert "release-manifest.txt" not in workflow
    assert "release-manifest.txt" not in publisher
    assert "dist/release-handoff.py" in workflow
    assert "qualified-release/release-handoff.py verify" in publisher
    assert "workflow_call:" in publisher
    assert "workflow_run:" not in publisher
    assert "actions/download-artifact@" in publisher
    assert "make release-check" not in publisher
    assert "apt-get" not in publisher
    assert "actions/checkout@" not in publisher
    assert "--method POST" not in publisher
    assert "git/refs" not in publisher
    assert "gh release create" in publisher
    assert '--target "$RELEASE_HEAD"' in publisher
    assert "gh release upload" in publisher
    assert "gh release edit" in publisher
    assert "bump VERSION instead of recovering the tag" in publisher

    # Qualification breadth remains explicit without making exact workflow text,
    # action counts or file counts part of the contract.
    for required_job in (
        "native:",
        "icu-compat:",
        "sanitizer:",
        "coverage:",
        "static-analysis:",
        "debian-package:",
        "bigbedroom-live-probe:",
        "package-install:",
        "publish-release:",
    ):
        assert required_job in workflow
    assert "runs-on: bigbedroom" in workflow
    assert "image: debian:12" in workflow
    assert "image: debian:13" in workflow
    assert "tools/live-cinnamon-ci-smoke.sh" in workflow
    assert (ROOT / ".github/workflows/upstream-drift.yml").is_file()

    assert_release_handoff_behaviour(version)

    # Licensing/provenance are release-input contracts, not workflow spelling.
    assert "Calendar is GPL-3.0-or-later" in read("README.md")
    assert "License: GPL-3+" in copyright_file
    project_owned_suffixes = {
        ".c", ".h", ".js", ".py", ".sh", ".in", ".yml", ".yaml"
    }
    for path in ROOT.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        if path.suffix not in project_owned_suffixes:
            continue
        content = path.read_text(encoding="utf-8")
        assert "SPDX-License-Identifier: GPL-3.0-or-later" in content, (
            f"missing GPL-3 SPDX marker: {path.relative_to(ROOT)}"
        )


if __name__ == "__main__":
    main()
