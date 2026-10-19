#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Behavioural contracts for Calendar's build and release model.

The suite protects release invariants rather than incidental workflow spelling.
Machine names, action counts and implementation-only step labels are deliberately
not part of the contract.
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
    match = re.match(r"calendar \(([^)]+)\) ", changelog)
    assert match, "unexpected debian/changelog header"
    return match.group(1)


def workflow_job(workflow: str, job: str, next_job: str) -> str:
    marker = f"  {job}:\n"
    next_marker = f"\n  {next_job}:\n"
    assert marker in workflow, f"missing workflow job {job}"
    section = workflow.split(marker, 1)[1]
    assert next_marker in section, f"missing workflow job boundary {next_job}"
    return section.split(next_marker, 1)[0]


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


def assert_release_intent_behaviour(version: str) -> None:
    tool = ROOT / "tools/release-handoff.py"

    def invoke(published: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [
                "python3",
                str(tool),
                "release-requested",
                "--published-version",
                published,
            ],
            cwd=ROOT,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    current = invoke(f"v{version}")
    assert current.returncode == 0, current.stderr
    assert "requested=false" in current.stdout
    assert f"version={version}" in current.stdout
    assert f"published_version={version}" in current.stdout

    major, minor, patch = (int(part) for part in version.split("."))
    if patch > 0:
        previous = f"{major}.{minor}.{patch - 1}"
    elif minor > 0:
        previous = f"{major}.{minor - 1}.0"
    elif major > 0:
        previous = f"{major - 1}.0.0"
    else:
        previous = ""

    unpublished = invoke(previous)
    assert unpublished.returncode == 0, unpublished.stderr
    assert "requested=true" in unpublished.stdout

    future = invoke(f"{major}.{minor}.{patch + 1}")
    assert future.returncode != 0
    assert "older than the latest published release" in future.stderr


def assert_project_license_markers(root: Path) -> None:
    """Apply Calendar's source-header policy only to Calendar-owned inputs."""
    project_owned_suffixes = {
        ".c", ".h", ".js", ".py", ".sh", ".in", ".yml", ".yaml"
    }
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if (
            ".git" in relative.parts
            or relative.parts[:2] == ("src", "vendor")
            or relative.parts[0] in {"build", "dist"}
        ):
            continue
        if path.suffix not in project_owned_suffixes:
            continue
        content = path.read_text(encoding="utf-8")
        assert "SPDX-License-Identifier: GPL-3.0-or-later" in content, (
            f"missing GPL-3 SPDX marker: {relative}"
        )


def assert_license_scope_behaviour() -> None:
    with tempfile.TemporaryDirectory(prefix="calendar-license-scope-") as tmp:
        root = Path(tmp)
        source = root / "src/calendar.c"
        source.parent.mkdir(parents=True)
        source.write_text("/* SPDX-License-Identifier: GPL-3.0-or-later */\n")
        for relative in (
            "src/vendor/shared/tools/dependency.py",
            "build/generated-probe.c",
            "dist/staged-helper.sh",
        ):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("# Dependency-owned or generated input\n")
        assert_project_license_markers(root)

        maintained = root / "tools/unmarked.py"
        maintained.parent.mkdir()
        maintained.write_text("# Maintained Calendar helper\n")
        try:
            assert_project_license_markers(root)
        except AssertionError as error:
            assert "tools/unmarked.py" in str(error)
        else:
            raise AssertionError("unmarked Calendar-owned source was accepted")


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

    version = read("VERSION").strip()
    assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version)
    assert "VERSION := $(strip $(file <VERSION))" in makefile
    assert "\tVERSION \\" in makefile
    assert re.search(r"^VERSION := [0-9]", makefile, re.MULTILINE) is None
    for version_consumer in (
        "tools/build-local-installer.sh",
        "tools/cinnamon-smoke.sh",
        "tools/isolated-cinnamon-artifact-smoke.sh",
        "tools/live-cinnamon-ci-smoke.sh",
        "tools/native-installer-smoke.sh",
        "tools/release-check.sh",
        "tools/reproducible-build.sh",
        "tools/validate-release-artifacts.sh",
    ):
        consumer = read(version_consumer)
        assert 'cat "$ROOT/VERSION"' in consumer or 'cat VERSION' in consumer
        assert "s/^VERSION :=" not in consumer
    assert metadata["version"] == version
    assert first_changelog_version(changelog) == version
    assert metadata["author"] == "Shannon Smith"
    assert metadata["website"] == "https://github.com/Infiltrator-Projects/Calendar"

    assert "BUILD_MODE ?= generic" in makefile
    assert "PGO_MODE ?= none" in makefile
    assert "-march=native" in makefile
    assert "CALENDAR_BUILD_MODE ?= generic" in rules
    assert "BUILD_MODE=$(CALENDAR_BUILD_MODE)" in rules
    assert "filter nocheck,$(DEB_BUILD_OPTIONS)" in rules
    assert "export CFLAGS" not in makefile

    common_version = make_var(makefile, "INFILTRATR_COMMON_VERSION")
    common_commit = make_var(makefile, "INFILTRATR_COMMON_COMMIT")
    assert re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", common_version)
    assert re.fullmatch(r"[0-9a-f]{40}", common_commit)
    common = ROOT / "src/vendor/infiltratr-common"
    assert (common / "VERSION").read_text(encoding="utf-8").strip() == common_version
    assert (common / "LICENSE").is_file()
    assert "libinfiltratr-common.a" in makefile
    assert "Infiltrator-Projects/Libraries.git" in read(".gitmodules")
    for private_source in ("core.c", "arithmetic.c", "timing.c", "dynlib.c"):
        assert f"$(INFILTRATR_COMMON_DIR)/src/{private_source}" not in makefile

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

    assert "FONT_ARCHIVE" not in makefile
    assert "prepare-fonts" not in makefile
    assert not (ROOT / "src/assets/fonts/mb-corpo-fonts.tar.xz").exists()
    common_design = json.loads(
        read("src/vendor/infiltratr-common/design/infiltrator-design-v1.json")
    )
    policy = common_design["policy"]
    assert policy["font_binaries_are_not_part_of_this_contract"] is True
    assert policy["fallback_is_required_when_mb_corpo_is_unavailable"] is True

    # Common alone owns provider source, module compilation and publication.
    assert "Source: calendar" in control
    assert "\nPackage: calendar\n" in control
    assert "\nPackage: infiltrator-calendar-resources\n" not in control
    assert "infiltrator-calendar-resources (= ${binary:Version})" not in control
    resource_version = make_var(makefile, "INFILTRATR_TEMPORAL_RESOURCES_VERSION")
    assert (
        f"Suggests: infiltrator-calendar-resources (>= {resource_version})"
        in control
    )
    assert "cinnamon (>= 6.4)" in control
    assert not (ROOT / "src/resources").exists()
    assert "RESOURCE_ENGINE_ARCHIVE" not in makefile
    assert "System/Resources" not in read("tools/release-check.sh")
    assert "github.com/Infiltrator-Projects/Libraries/releases/download" in read("tools/test-resources.sh")
    assert "validate-runtime-deps:" in makefile

    assert 'NATIVE_VERSION="${VERSION}+nativepgo${NATIVE_REVISION}"' in installer
    assert "CALENDAR_BUILD_MODE=native" in installer
    assert "PGO_MODE=generate" in installer
    assert "PGO_MODE=use" in installer
    assert "--build-only" in installer
    assert "--verify-only" in installer
    assert "@PAYLOAD_SHA256@" in installer
    assert '(cd "$SOURCE_DIR" && dpkg-checkbuilddeps)' in installer

    assert "CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED" in release_check
    assert "make reproducible-build" in release_check
    assert "tools/native-installer-smoke.sh" in release_check
    assert "tools/validate-release-artifacts.sh" in release_check
    assert "infiltrator-calendar-resources_${VERSION}_${ARCH}.deb" not in release_check
    assert "lintian --fail-on error" in release_check

    assert "release-handoff.py release-requested" in workflow
    assert "--published-version" in workflow
    assert "releases?per_page=100" in workflow
    assert "select(.draft == false and .prerelease == false)" in workflow
    assert "sort -V | tail -n 1" in workflow
    assert "releases/latest" not in workflow
    assert "github.event.before" not in workflow
    assert "fetch-depth: 0" not in workflow
    assert "release_requested:" in workflow
    assert "needs.debian-package.outputs.release_requested == 'true'" in workflow
    assert "head_commit.message" not in workflow
    assert "startsWith(" not in workflow
    assert "cancel-in-progress: false" in workflow

    # Lifecycle qualification proves application removal cannot remove resources.
    package_install = workflow_job(workflow, "package-install", "publish-release")
    assert "needs: [debian-package]" in package_install
    assert "actions/download-artifact@" in package_install
    assert "qualified-release/release-handoff.py verify" in package_install
    assert '"$RELEASE_RESOURCES_DEB" "./$RELEASE_DEB"' in package_install
    assert "apt-get purge -y calendar" in package_install
    assert "infiltrator-calendar-resources" in package_install
    assert 'test -f "/System/Resources/Calendars/Hebrew Calendar.so"' in package_install
    assert "apt-get purge -y infiltrator-calendar-resources" in package_install
    assert "dpkg-buildpackage" not in package_install

    live_cinnamon = workflow_job(workflow, "live-cinnamon", "package-install")
    assert "runs-on: ubuntu-24.04" in live_cinnamon
    assert "image: debian:13" in live_cinnamon
    assert "cinnamon dbus-x11" in live_cinnamon
    assert "actions/download-artifact@" in live_cinnamon
    assert "tools/isolated-cinnamon-artifact-smoke.sh" in live_cinnamon
    assert "bigbedroom" not in workflow.lower()

    assert "release-manifest.json" in workflow
    assert "release-manifest.txt" not in workflow
    assert "release-manifest.txt" not in publisher
    assert "dist/release-handoff.py" in workflow
    assert "dist/infiltrator-calendar-resources_*_amd64.deb" not in workflow
    assert "qualified-release/release-handoff.py verify" in publisher
    assert "RELEASE_RESOURCES_DEB" not in publisher
    assert "workflow_call:" in publisher
    assert "workflow_run:" not in publisher
    assert "actions/download-artifact@" in publisher
    assert "make release-check" not in publisher
    assert "apt-get" not in publisher
    assert "actions/checkout@" not in publisher
    assert "releases?per_page=100" in publisher
    assert "RELEASE_ID" in publisher
    assert "--method POST" in publisher
    assert "uploads.github.com" in publisher
    assert "-F draft=false" in publisher
    assert "git/refs" not in publisher
    assert "gh release create" not in publisher
    assert "gh release upload" not in publisher
    assert "gh release edit" not in publisher
    assert "bump VERSION instead of recovering the tag" in publisher
    assert '[[ "$remote_digest" != "sha256:${digest}" ]]' in publisher
    assert '[[ -n "$remote_digest"' not in publisher
    publish_index = publisher.index("-F draft=false")
    tag_verify_index = publisher.index(
        "git/ref/tags/${RELEASE_TAG}", publish_index
    )
    assert publish_index < tag_verify_index

    for required_job in (
        "native:",
        "icu-compat:",
        "sanitizer:",
        "coverage:",
        "static-analysis:",
        "debian-package:",
        "live-cinnamon:",
        "package-install:",
        "publish-release:",
    ):
        assert required_job in workflow
    assert "image: debian:12" in workflow
    assert "image: debian:13" in workflow
    assert "tools/live-cinnamon-ci-smoke.sh" in workflow
    assert (ROOT / ".github/workflows/upstream-drift.yml").is_file()

    assert_release_intent_behaviour(version)
    assert_release_handoff_behaviour(version)

    assert "Calendar is GPL-3.0-or-later" in read("README.md")
    assert "License: GPL-3+" in copyright_file
    assert_license_scope_behaviour()
    assert_project_license_markers(ROOT)


if __name__ == "__main__":
    main()
