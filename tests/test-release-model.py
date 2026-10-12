#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Static contracts for Calendar's externally observable release model.

The suite protects release identities, package boundaries and publication
invariants without freezing the internal spelling of shell commands or workflow
steps. Behavioural and architecture contracts belong to their dedicated tests.
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
    release_check = read("tools/release-check.sh")
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
        "ARCHITECTURE.md", "DECISIONS.md", "DESIGN.md", "MODELS.md",
        "PORTABILITY.md", "QUALIFICATION.md", "README.md", "ROADMAP.md",
        "VALIDATION.md",
    ]
    assert (ROOT / ".github/ISSUE_TEMPLATE/bug_report.yml").is_file()
    assert (ROOT / ".github/ISSUE_TEMPLATE/feature_request.yml").is_file()
    assert "## Range and continuation matrix" in read("docs/MODELS.md")
    assert "## Current development boundary" in read("docs/QUALIFICATION.md")
    assert (ROOT / "LICENSE").is_file()
    assert not (ROOT / "COPYING").exists()
    assert "DEVELOPING.md" not in makefile
    assert "install -m644 README.md" in makefile

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

    # Build and package identities.
    assert "BUILD_MODE ?= generic" in makefile
    assert "-O2 -g" in makefile
    assert "-O3 -g -march=native -mtune=native -flto=auto" in makefile
    assert "PGO_MODE ?= none" in makefile
    assert "-fprofile-generate=$(PGO_DIR)" in makefile
    assert "-fprofile-use=$(PGO_DIR)" in makefile
    assert "Calendar-$(VERSION)-local-source.tar.gz" in makefile
    assert "Calendar-$(VERSION)-local-source.zip" not in makefile
    assert "calendar-$(VERSION)-local-folder.run" in makefile
    assert "calendar-plus-about" in makefile
    assert "export CFLAGS" not in makefile

    # Common remains an independently versioned, pinned build boundary.
    assert "INFILTRATR_COMMON_CFLAGS :=" in makefile
    assert "INFILTRATR_COMMON_EXTRA_CFLAGS ?=" in makefile
    assert "INFILTRATR_COMMON_CPPFLAGS := -Iinclude" in makefile
    assert "libinfiltratr-common.a" in makefile
    assert "src/vendor/infiltratr-common" in makefile
    assert (
        "INFILTRATR_COMMON_COMMIT := "
        "b0387b7ab5d71cda60748e2506b85cdc0e6b5e8b"
    ) in makefile
    assert "INFILTRATR_COMMON_VERSION := 1.19.41" in makefile
    assert "common-bootstrap: common-check" in makefile
    assert "common-test: $(INFILTRATR_COMMON_ARCHIVE)" in makefile
    assert "Infiltrator-Projects/Infiltrator-Libraries.git" in gitmodules
    for private_source in ("core.c", "arithmetic.c", "timing.c", "dynlib.c"):
        assert f"$(INFILTRATR_COMMON_DIR)/src/{private_source}" not in makefile

    # Proprietary font binaries are never release inputs.
    assert "FONT_ARCHIVE" not in makefile
    assert "prepare-fonts" not in makefile
    assert "share/fonts/truetype/infiltrator-calendar" not in makefile
    assert not (ROOT / "src/assets/fonts/mb-corpo-fonts.tar.xz").exists()
    assert not (ROOT / "tools/prepare-fonts.py").exists()
    assert "does not redistribute proprietary MB Corpo font binaries" in read("README.md")
    assert "License: proprietary" not in copyright_file

    metadata = json.loads(read("src/cinnamon/metadata.json"))
    assert "external-configuration-app" not in metadata
    assert metadata["icon"] == "infiltratr-calendar"
    assert (ROOT / "src/assets/infiltratr-calendar.png").is_file()
    assert "share/icons/hicolor/256x256/apps/$(ICON_NAME).png" in makefile
    assert "share/app-install/icons/infiltrator-calendar.png" in makefile
    assert "share/cinnamon/applets/$(UUID)/icon.png" in makefile
    package_validator = read("tools/validate-package-inputs.py")
    assert "validate_icon_asset()" in package_validator
    assert "4181adea531fe79e2e8b79ec14e2f2476d18e9e3128911dcd21e60a863b36f05" in package_validator

    stylesheet = read("src/cinnamon/stylesheet.css")
    assert "MB Corpo S Title WEB" in stylesheet
    assert '.calendar-plus-panel-clock {' in stylesheet

    settings_schema = read("src/cinnamon/settings-schema.json")
    for temporal_key in (
        "follow-system-temporal",
        "clock-mode",
        "show-seconds",
        "location-configured",
        "latitude",
        "longitude",
        "primary-calendar",
        "secondary-calendar",
        "use-custom-format",
        "custom-format",
        "custom-tooltip-format",
    ):
        assert temporal_key not in settings_schema
    assert not (ROOT / "src/cinnamon/settings.py").exists()

    # Debian package identity and release artifact contracts.
    assert "fontconfig," not in control
    assert "Breaks: calendar-plus, cinnamon-calendar" in control
    assert "Replaces: calendar-plus, cinnamon-calendar" in control
    assert "CALENDAR_PLUS_BUILD_MODE ?= generic" in rules
    assert "BUILD_MODE=$(CALENDAR_PLUS_BUILD_MODE)" in rules
    assert "filter nocheck,$(DEB_BUILD_OPTIONS)" in rules
    assert "validate-translations" in rules
    assert "validate-architecture" in rules
    assert "validate-abi" in rules
    assert "validate-runtime-deps" in rules

    translation_tool = read("tools/update-translations.py")
    assert '"msgmerge", "--update", "--backup=none"' in translation_tool
    assert "missing maintained translation catalogue" in translation_tool

    assert 'NATIVE_VERSION="${VERSION}+nativepgo${NATIVE_REVISION}"' in installer
    assert "CALENDAR_PLUS_BUILD_MODE=native" in installer
    assert "PGO_MODE=generate" in installer
    assert "PGO_MODE=use" in installer
    assert "--build-only" in installer
    assert "--verify-only" in installer
    assert "@PAYLOAD_SHA256@" in installer

    assert 'ARCH" = "amd64"' in release_check
    assert "CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED" in release_check
    assert "make reproducible-build" in release_check
    assert "tools/native-installer-smoke.sh" in release_check
    assert "tools/validate-release-artifacts.sh" in release_check
    assert "lintian --fail-on error" in release_check
    assert "local-source.zip" not in release_check
    reproducible = read("tools/reproducible-build.sh")
    assert "nocheck" in reproducible
    assert 'DEB="$DIST/calendar_${VERSION}_${ARCH}.deb"' in artifact_validator
    assert 'RUN="$DIST/calendar-${VERSION}-local-folder.run"' in artifact_validator
    assert "exactly two files" in artifact_validator
    assert "build-profile=native" in native_installer_smoke

    # Release publication is a final stage of the same CI run. It consumes an
    # immutable handoff manifest and never rebuilds or re-qualifies the source.
    assert "workflow_call:" in publisher
    assert "workflow_run:" not in publisher
    assert "apt-get" not in publisher
    assert "make release-check" not in publisher
    assert "origin/main" not in publisher
    assert "git fetch origin main" not in publisher
    assert "release-manifest.txt" in publisher
    assert "release-notes.md" in publisher
    assert "actions/download-artifact@" in publisher
    assert "sha256sum" in publisher
    assert "Published releases are immutable" in publisher
    assert "local-source.zip" not in publisher

    assert "publish-release:" in workflow
    assert "uses: ./.github/workflows/release.yml" in workflow
    assert "calendar-release-${{ github.sha }}" in workflow
    assert "release-manifest.txt" in workflow
    assert "release-notes.md" in workflow
    assert "Create immutable release handoff" in workflow
    assert "release-policy:" not in workflow
    assert "grep -Fq" not in workflow

    # Qualification breadth remains explicit even though publication is simpler.
    assert "actions/checkout@v" not in workflow
    assert workflow.count("submodules: true") == 7
    assert "schedule:" not in workflow
    assert (ROOT / ".github/workflows/upstream-drift.yml").is_file()
    assert (ROOT / "tools/check-upstream-calendar-drift.py").is_file()
    assert (ROOT / "tools/upstream-calendar-baseline.json").is_file()
    assert "tools/live-cinnamon-ci-smoke.sh" in workflow
    assert "runs-on: bigbedroom" in workflow
    assert "image: debian:12" in workflow
    assert "icu_major: 72" in workflow
    assert "image: debian:13" in workflow
    assert "icu_major: 76" in workflow
    assert "Migrate retired package identities" in workflow
    assert '"calendar-plus 3.6.0"' in workflow
    assert '"cinnamon-calendar 1.0.16"' in workflow

    live_smoke = read("tools/live-cinnamon-ci-smoke.sh")
    assert "dpkg-buildpackage -us -uc -b" in live_smoke
    assert "sudo -n apt-get install -y --no-install-recommends" in live_smoke
    assert "Extension.reloadExtension" in live_smoke
    assert "--probe" in live_smoke
    assert "live smoke deferred" not in live_smoke

    # Repository shape and generated/runtime release inputs.
    assert not (ROOT / "calendar-plus@the-infiltratr").exists()
    for legacy_root in ("cinnamon", "po", "shared"):
        assert not (ROOT / legacy_root).exists()
    assert (ROOT / "src/cinnamon/runtime-sources.sha256").is_file()
    assert (ROOT / "src/i18n/LINGUAS").is_file()
    assert {
        line.strip()
        for line in read("src/i18n/LINGUAS").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    } == {"de", "en_AU", "en_GB", "en_US", "es", "fr", "it", "ja", "pt_BR", "ru"}
    assert (ROOT / "src/vendor/infiltratr-common").is_dir()
    source_root = ROOT / "src"
    assert not [path for path in source_root.iterdir() if path.is_file()]
    assert sorted(path.name for path in source_root.iterdir() if path.is_dir()) == [
        "abi", "adapters", "app", "assets", "cinnamon", "core", "i18n", "vendor"
    ]
    assert "update-runtime-hashes.py --check" in makefile
    assert "tests/test-abi.py --library" in makefile
    assert "validate-runtime-deps:" in makefile
    assert "Generic runtime must not embed an ICU-major DT_NEEDED entry" in makefile

    assert "COVERAGE_MIN_LINES ?= 80" in makefile
    assert "COVERAGE_MIN_BRANCHES ?= 60" in makefile
    assert "--fail-under-line $(COVERAGE_MIN_LINES)" in makefile
    assert "--fail-under-branch $(COVERAGE_MIN_BRANCHES)" in makefile
    assert "clang-analyzer-*" in makefile
    assert (ROOT / "tests/abi-baseline.txt").is_file()

    # Calendar may call only APIs present in the pinned Common public headers.
    common = ROOT / "src/vendor/infiltratr-common"
    assert (common / "VERSION").read_text(encoding="utf-8").strip() == "1.19.41"
    assert (common / "LICENSE").is_file()
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
        function for function in calendar_common_calls
        if not re.search(rf"\b{re.escape(function)}\s*\(", public_api)
    )
    assert not missing_common_api, (
        "Calendar calls APIs absent from pinned Common public headers: "
        f"{missing_common_api}"
    )

    common_design = json.loads(
        read("src/vendor/infiltratr-common/design/infiltrator-design-v1.json")
    )
    typography = common_design["typography"]
    design_policy = common_design["policy"]
    assert design_policy["font_binaries_are_not_part_of_this_contract"] is True
    assert design_policy["fallback_is_required_when_mb_corpo_is_unavailable"] is True
    assert typography["ui_family"] in stylesheet
    assert typography["brand_family"] in stylesheet

    # Runtime dependency and version identity.
    assert "cinnamon (>= 6.4)" in control
    for icu_major in range(72, 81):
        assert f"libicu{icu_major}" in control
    binary_stanza = control.split("\nPackage: infiltrator-calendar\n", 1)[1]
    runtime_control = binary_stanza.split("Description:", 1)[0]
    assert "libgtk-3-0," not in runtime_control
    assert "\n python3," not in runtime_control
    assert "Source: infiltrator-calendar" in control
    assert "Package: infiltrator-calendar" in control
    assert "Package: calendar-plus" not in control
    assert "-DU_DISABLE_RENAMING=1" in makefile
    assert "src/core/icu-compat-bridge.c" in makefile
    assert "ICU_BRIDGE_LIBS = $(DYNLIB_LIBS) -pthread" in makefile
    assert "DYNLIB_LIBS = -ldl" in makefile

    version = metadata["version"]
    extracted_version = subprocess.run(
        ["sed", "-n", "s/^VERSION := //p", "Makefile"],
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout.strip()
    assert extracted_version == version
    assert f"VERSION := {version}" in makefile
    assert metadata["author"] == "Shannon Smith"
    assert metadata["website"] == "https://github.com/Infiltrator-Projects/Calendar"

    subprocess.run(
        ["dpkg", "--compare-versions", f"{version}+nativepgo1", "gt", version],
        check=True,
    )

    # Licensing and release-input provenance.
    assert ("GPL-" + "2.0-or-later") not in read("README.md")
    assert "Calendar is GPL-3.0-or-later" in read("README.md")
    assert "License: GPL-3+" in copyright_file
    assert "/usr/share/common-licenses/GPL-3" in copyright_file

    project_owned_suffixes = {".c", ".h", ".js", ".py", ".sh", ".in", ".yml", ".yaml"}
    for path in ROOT.rglob("*"):
        if not path.is_file() or ".git" in path.parts:
            continue
        if path.suffix not in project_owned_suffixes:
            continue
        content = path.read_text(encoding="utf-8")
        assert ("GPL-" + "2.0-or-later") not in content, path.relative_to(ROOT)
        assert "SPDX-License-Identifier: GPL-3.0-or-later" in content, (
            f"missing GPL-3 SPDX marker: {path.relative_to(ROOT)}"
        )

    forbidden = ("Chat" + "GPT", "Open" + "AI", "H" + "AL 9000")
    for path in ROOT.rglob("*"):
        if (not path.is_file() or ".git" in path.parts or
                path.parts[-2:-1] in (("build",), ("dist",))):
            continue
        if path.suffix not in {
            ".c", ".h", ".js", ".json", ".md", ".py", ".sh", ".in",
            ".rules", ".yml", ".yaml",
        } and path.name not in {"Makefile", "changelog", "control", "copyright"}:
            continue
        content = path.read_text(encoding="utf-8")
        stale_copyright = "Copyright (C) " + "2026 Shannon Smith"
        assert stale_copyright not in content, (
            f"stale project copyright span: {path.relative_to(ROOT)}"
        )
        assert not any(word in content for word in forbidden), (
            f"assistant attribution in release input: {path.relative_to(ROOT)}"
        )


if __name__ == "__main__":
    main()
