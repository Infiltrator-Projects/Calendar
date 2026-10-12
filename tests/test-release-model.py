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
        "ARCHITECTURE.md", "DECISIONS.md", "DESIGN.md", "MODELS.md",
        "PORTABILITY.md", "QUALIFICATION.md", "README.md", "ROADMAP.md", "VALIDATION.md"
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
    assert "pgo-train: $(BUILD_DIR)/pgo-train" in makefile
    assert (ROOT / "tools/pgo-train.c").is_file()
    assert "override CFLAGS += $(NATIVE_CFLAGS) $(PGO_CFLAGS) $(CALENDAR_CFLAGS)" in makefile
    assert "Calendar-$(VERSION)-local-source.tar.gz" in makefile
    assert "Calendar-$(VERSION)-local-source.zip" not in makefile
    assert "TZ=UTC zip -X -9 -q" not in makefile
    assert "export CFLAGS" not in makefile

    # Common is consumed as an independently built pinned library. These checks
    # protect the build boundary rather than the spelling of Calendar callers.
    assert "INFILTRATR_COMMON_CFLAGS :=" in makefile
    assert "INFILTRATR_COMMON_EXTRA_CFLAGS ?=" in makefile
    assert "INFILTRATR_COMMON_CPPFLAGS := -Iinclude" in makefile
    assert 'INFILTRATR_COMMON_EXTRA_CFLAGS="$(SANITIZER_CFLAGS)"' in makefile
    assert "Sanitizer instrumentation is missing from Infiltratr Common." in makefile
    assert makefile.count(
        '\n\t\tCPPFLAGS="$(INFILTRATR_COMMON_CPPFLAGS)" \\\n'
        "\t\tCFLAGS='$(INFILTRATR_COMMON_CFLAGS)' \\\n"
    ) == 2
    assert "\n\tCFLAGS= \\\n\t$(G_IR_SCANNER) \\" in makefile
    assert "\n\t.gitmodules \\\n" in makefile
    assert "\n\tCHANGELOG.md \\\n" in makefile
    assert "\n\tCONTRIBUTING.md \\\n" in makefile
    assert "\n\tSECURITY.md \\\n" in makefile
    assert "\n\tdocs \\\n" in makefile
    assert "--exclude='*/.git'" in makefile
    assert "--exclude-vcs" not in read("tools/reproducible-build.sh")
    assert "calendar-$(VERSION)-local-folder.run" in makefile
    assert "calendar-plus-about" in makefile

    # Proprietary font binaries are never release inputs. The stylesheet may
    # request the brand family, but packaging must work with documented fallback.
    assert "FONT_ARCHIVE" not in makefile
    assert "prepare-fonts" not in makefile
    assert "share/fonts/truetype/infiltrator-calendar" not in makefile
    assert not (ROOT / "src/assets/fonts/mb-corpo-fonts.tar.xz").exists()
    assert not (ROOT / "tools/prepare-fonts.py").exists()
    assert "does not redistribute proprietary MB Corpo font binaries" in read("README.md")
    assert "src/assets/fonts/mb-corpo-fonts.tar.xz" not in copyright_file
    assert "License: proprietary" not in copyright_file
    assert "Files: src/cinnamon/settings-schema.json" in copyright_file
    assert "Linux Mint Project and Cinnamon contributors" in copyright_file
    assert "CHANGELOG.md CONTRIBUTING.md LICENSE Makefile README.md SECURITY.md docs/*" in copyright_file
    assert "3.4.0" not in copyright_file

    metadata = json.loads(read("src/cinnamon/metadata.json"))
    assert "external-configuration-app" not in metadata
    assert metadata["icon"] == "infiltratr-calendar"
    assert (ROOT / "src/assets/infiltratr-calendar.png").is_file()
    assert "share/icons/hicolor/256x256/apps/$(ICON_NAME).png" in makefile
    assert "share/app-install/icons/infiltrator-calendar.png" in makefile
    assert "share/cinnamon/applets/$(UUID)/icon.png" in makefile
    package_validator = read("tools/validate-package-inputs.py")
    assert "validate_icon_asset()" in package_validator
    assert '4181adea531fe79e2e8b79ec14e2f2476d18e9e3128911dcd21e60a863b36f05' in package_validator
    local_installer = read("tools/local-installer.sh.in")
    assert "share/cinnamon/applets/calendar-plus@the-infiltratr/icon.png" in local_installer
    assert "share/icons/hicolor/256x256/apps/infiltratr-calendar.png" in local_installer
    assert "share/app-install/icons/infiltrator-calendar.png" in local_installer
    assert 'test ! -e "$PACKAGE_ROOT/usr/share/fonts/truetype/infiltrator-calendar"' in local_installer

    stylesheet = read("src/cinnamon/stylesheet.css")
    assert "MB Corpo S Title WEB" in stylesheet
    assert '.calendar-plus-panel-clock {' in stylesheet
    assert 'font-family: "MB Corpo S Title WEB";' in stylesheet
    assert "font-weight: 400;" in stylesheet
    assert "font-size: 1.08em;" in stylesheet
    assert ".calendar-day-heading {" in stylesheet
    assert "font-size: 10px;" in stylesheet

    # Temporal settings are owned by System Settings/Mint, not duplicated in
    # Calendar's installed Cinnamon schema. Runtime behaviour is tested by the
    # temporal-policy and JavaScript behavioural suites.
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

    assert "fontconfig," not in control
    assert "Breaks: calendar-plus, cinnamon-calendar" in control
    assert "Replaces: calendar-plus, cinnamon-calendar" in control
    assert "calendar-plus (<<" not in control
    assert "cinnamon-calendar (<<" not in control
    assert "libinfiltratr-common.a" in makefile
    assert "src/vendor/infiltratr-common" in makefile
    assert (
        "INFILTRATR_COMMON_COMMIT := "
        "b0387b7ab5d71cda60748e2506b85cdc0e6b5e8b"
    ) in makefile
    assert "INFILTRATR_COMMON_VERSION := 1.19.41" in makefile
    assert "normal `make` automatically retrieves" in read("README.md")
    assert "common-bootstrap: common-check" in makefile
    assert "common-test: $(INFILTRATR_COMMON_ARCHIVE)" in makefile
    assert 'BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)" check' in makefile
    assert "git -C \"$(INFILTRATR_COMMON_DIR)\" fetch -q --depth 1 origin" in makefile
    assert "Infiltrator-Projects/Infiltrator-Libraries.git" in gitmodules

    assert "validate-translations" in makefile
    translation_tool = read("tools/update-translations.py")
    assert '"msgmerge", "--update", "--backup=none"' in translation_tool
    assert "missing maintained translation catalogue" in translation_tool
    assert "CALENDAR_PLUS_BUILD_MODE ?= generic" in rules
    assert "BUILD_MODE=$(CALENDAR_PLUS_BUILD_MODE)" in rules
    assert "DEB_BUILD_MAINT_OPTIONS := optimize=-lto" in rules
    assert "DEB_BUILD_MAINT_OPTIONS := optimize=+lto" in rules
    assert "CHANGELOG.md" not in rules
    assert "validate-translations" in rules
    assert "validate-architecture" in rules
    assert "validate-abi" in rules
    assert "validate-runtime-deps" in rules

    # Common is an independently versioned library. Calendar may include its
    # public headers and link its public archive, but must never encode Common's
    # private source-file membership in this repository.
    assert "INFILTRATR_COMMON_SOURCES :=" not in makefile
    assert "INFILTRATR_COMMON_OBJECTS :=" not in makefile
    assert "$(BUILD_DIR)/infiltratr-%.o" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/core.c" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/arithmetic.c" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/timing.c" not in makefile
    assert "$(INFILTRATR_COMMON_DIR)/src/dynlib.c" not in makefile
    assert "INFILTRATR_COMMON_BUILD_DIR :=" in makefile
    assert '$(MAKE) -C "$(INFILTRATR_COMMON_DIR)"' in makefile
    assert 'BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)"' in makefile

    # Native installer and release workflow identity. Exact commands matter here
    # because these strings describe the artifacts users install and publish.
    assert 'NATIVE_VERSION="${VERSION}+nativepgo${NATIVE_REVISION}"' in installer
    assert "CALENDAR_PLUS_BUILD_MODE=native" in installer
    assert "PGO_MODE=generate" in installer
    assert "PGO_MODE=use" in installer
    assert "pgo-train" in installer
    assert "dpkg-buildpackage -us -uc -b" in installer
    assert "apt-get install -y --allow-downgrades \"$OUTPUT_DEB\"" in installer
    assert "--build-only" in installer
    assert "--verify-only" in installer
    assert "@PAYLOAD_SHA256@" in installer
    assert 'cat "$SOURCE_TAR" >> "$TEMP_OUTPUT"' in builder

    assert 'ARCH" = "amd64"' in release
    assert 'CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED' in release
    assert 'DEB_BUILD_OPTIONS' in release and 'nocheck' in release
    assert 'filter nocheck,$(DEB_BUILD_OPTIONS)' in rules
    assert 'DEB_BUILD_OPTIONS="parallel=$JOBS nocheck"' in installer
    assert 'BUILD_MODE=native PGO_MODE=generate PGO_DIR="$PGO_DIR" \\\n        all\n' in installer
    assert 'BUILD_MODE=native PGO_MODE=generate PGO_DIR="$PGO_DIR" \\\n        all test\n' not in installer
    reproducible = read("tools/reproducible-build.sh")
    assert 'nocheck' in reproducible
    assert 'CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED: "1"' in workflow
    assert 'DEB_BUILD_OPTIONS: nocheck' in workflow
    assert 'CALENDAR_RELEASE_SOURCE_ALREADY_QUALIFIED=1 make release-check' in publisher
    assert "make reproducible-build" in release
    assert "tools/native-installer-smoke.sh" in release
    assert "tools/validate-release-artifacts.sh" in release
    assert "infiltratr-calendar.png" in artifact_validator
    assert "lintian --fail-on error" in release
    assert "local-source.zip" not in release
    assert 'dist/calendar_${VERSION}_${ARCH}.deb' in release
    assert 'DEB="../infiltrator-calendar_${VERSION}_${ARCH}.deb"' in release
    assert 'dist/calendar-${VERSION}-local-folder.run' in release
    assert 'DEB="$DIST/calendar_${VERSION}_${ARCH}.deb"' in artifact_validator
    assert 'build-profile=generic' in artifact_validator
    assert 'RUN="$DIST/calendar-${VERSION}-local-folder.run"' in artifact_validator
    assert 'calendar-${VERSION}-local-folder.run' in native_installer_smoke
    assert 'calendar_${NATIVE_VERSION}_${ARCH}.deb' in native_installer_smoke
    assert 'build-profile=native' in native_installer_smoke
    assert '"$(dpkg-deb -f "$DEB" Package)" = "infiltrator-calendar"' in native_installer_smoke
    assert 'deb="dist/calendar_${version}_amd64.deb"' in publisher
    assert 'installer="dist/calendar-${version}-local-folder.run"' in publisher
    assert "exactly two files" in artifact_validator
    assert 'gh release create "$tag"' in publisher
    assert 'gh release upload "$tag" "$deb" "$installer" --clobber' in publisher
    assert 'gh release edit "$tag" --draft=false --latest' in publisher
    assert "baseline_reset" not in publisher
    assert 'git push --force origin "$tag"' not in publisher
    assert 'release_was_published=false' in publisher
    assert 'release_was_published=true' in publisher
    assert 'if [[ "$release_was_published" == false ]]; then' in publisher
    assert 'version=$(sed -n \'s/^VERSION := //p\' Makefile)' in publisher
    assert '< VERSION' not in publisher
    assert "/dispatches" not in publisher
    assert "client_payload" not in publisher
    assert "cancel-in-progress: true" in publisher
    assert "Verify Infiltrator APT repository publication" not in publisher
    assert "Infiltrator-Repository/catalogue/apps.json" not in publisher
    assert "seq 1 180" not in publisher
    assert "within 30 minutes" not in publisher
    assert "--draft" in publisher
    assert "release_state=" in publisher
    assert '--json databaseId --jq .databaseId' in publisher
    assert "local-source.zip" not in publisher

    # CI qualification matrix is itself a release contract: only a tested main
    # build may publish, and the supported ICU/Cinnamon environments must remain
    # represented explicitly.
    assert "actions/checkout@v" not in workflow
    assert workflow.count("submodules: true") == 7
    assert "check-upstream-drift.sh" not in workflow
    assert "upstream-drift:" not in workflow
    assert "schedule:" not in workflow
    assert (ROOT / ".github/workflows/upstream-drift.yml").is_file()
    assert (ROOT / "tools/check-upstream-calendar-drift.py").is_file()
    assert (ROOT / "tools/upstream-calendar-baseline.json").is_file()
    assert (ROOT / "tools/live-cinnamon-ci-smoke.sh").is_file()
    assert "tools/live-cinnamon-ci-smoke.sh" in workflow
    assert "runs-on: bigbedroom" in workflow
    assert "image: debian:12" in workflow
    assert "icu_major: 72" in workflow
    assert "image: debian:13" in workflow
    assert "icu_major: 76" in workflow
    assert "Probe exact-version live Cinnamon qualification" in workflow
    assert "steps.live-cinnamon.outputs.ready == 'true'" in workflow
    assert 'build-profile=generic' in workflow
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
    assert not (ROOT / "src/i18n/POTFILES.in").exists()
    assert not (ROOT / "src/i18n/settings-strings.c").exists()
    assert not (ROOT / "tests/exported-symbols.txt").exists()
    assert "update-runtime-hashes.py --check" in makefile
    assert "tests/test-abi.py --library" in makefile
    assert "validate-runtime-deps:" in makefile
    assert "Generic runtime must not embed an ICU-major DT_NEEDED entry" in makefile
    assert "forbidden compatibility library" in artifact_validator
    assert "libcalendar-base.so.0" in artifact_validator
    assert "libcpicu.so.0" in artifact_validator
    assert "## Native ABI and compatibility" in read("docs/ARCHITECTURE.md")

    # Coverage/static-analysis thresholds are release gates, not implementation
    # source checks, so they remain explicit here.
    assert "COVERAGE_MIN_LINES ?= 80" in makefile
    assert "COVERAGE_MIN_BRANCHES ?= 60" in makefile
    assert "--fail-under-line $(COVERAGE_MIN_LINES)" in makefile
    assert "--fail-under-branch $(COVERAGE_MIN_BRANCHES)" in makefile
    assert "clang-analyzer-*" in makefile
    assert (ROOT / "tests/abi-baseline.txt").is_file()

    # Validate Calendar's actual Common calls against Common's complete public
    # header surface. This derives the contract semantically instead of freezing
    # a list of Calendar implementation helpers or Common private source files.
    common = ROOT / "src/vendor/infiltratr-common"
    assert (common / "VERSION").read_text(encoding="utf-8").strip() == "1.19.41"
    assert (common / "LICENSE").is_file()
    common_include = common / "include/infiltratr"
    for public_header in (
        "core.h", "arithmetic.h", "timing.h", "temporal.h", "dynlib.h", "design.h", "utf8.h"
    ):
        assert (common_include / public_header).is_file()
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
    assert calendar_common_calls, "Calendar no longer exercises Common public APIs"
    missing_common_api = sorted(
        function for function in calendar_common_calls
        if not re.search(rf"\b{re.escape(function)}\s*\(", public_api)
    )
    assert not missing_common_api, (
        "Calendar calls APIs absent from pinned Common public headers: "
        f"{missing_common_api}"
    )

    # Generated design tokens are package data derived from the pinned Common
    # contract. Check the generated output, not the JS/C implementation route.
    common_design = json.loads(
        read("src/vendor/infiltratr-common/design/infiltrator-design-v1.json")
    )
    typography = common_design["typography"]
    design_policy = common_design["policy"]
    assert design_policy["font_binaries_are_not_part_of_this_contract"] is True
    assert design_policy["fallback_is_required_when_mb_corpo_is_unavailable"] is True
    assert typography["ui_family"] in stylesheet
    assert typography["brand_family"] in stylesheet
    common_metrics = common_design["metrics"]
    assert "BEGIN GENERATED COMMON TYPOGRAPHY TOKENS" in stylesheet
    assert "BEGIN GENERATED COMMON METRIC TOKENS" in stylesheet
    assert f'border-radius: {common_metrics["panel_radius"]}px;' in stylesheet
    assert f'spacing: {common_metrics["section_spacing"]}px;' in stylesheet
    assert f'border-radius: {common_metrics["card_radius"]}px;' in stylesheet
    assert f'border-radius: {common_metrics["control_radius"]}px;' in stylesheet
    assert f'border-radius: {common_metrics["small_radius"]}px;' in stylesheet

    # Runtime dependency/package metadata.
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
    assert "Breaks: calendar-plus, cinnamon-calendar" in control
    assert "Replaces: calendar-plus, cinnamon-calendar" in control
    assert "$(PKG_CONFIG) --cflags gio-2.0 gobject-2.0 icu-i18n)" in makefile
    assert "$(PKG_CONFIG) --libs gio-2.0 gobject-2.0)" in makefile
    assert "-DU_DISABLE_RENAMING=1" in makefile
    assert "src/core/icu-compat-bridge.c" in makefile
    assert "ICU_BRIDGE_LIBS = $(DYNLIB_LIBS) -pthread" in makefile
    assert "DYNLIB_LIBS = -ldl" in makefile

    # Version identity must agree across build metadata and Cinnamon metadata.
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
    subprocess.run(
        [
            "dpkg", "--compare-versions",
            ".".join([*version.split(".")[:-1], str(int(version.split(".")[-1]) + 1)]),
            "gt", f"{version}+nativepgo1"
        ],
        check=True,
    )

    # Licensing and release-input provenance.
    assert ("GPL-" + "2.0-or-later") not in read("README.md")
    assert "Calendar is GPL-3.0-or-later" in read("README.md")
    assert "License: GPL-3+" in read("debian/copyright")
    assert "/usr/share/common-licenses/GPL-3" in read("debian/copyright")
    assert "GNU GENERAL PUBLIC LICENSE" in read(
        "src/vendor/infiltratr-common/LICENSE"
    )
    assert "BSD 3-Clause License" not in read("src/vendor/infiltratr-common/LICENSE")

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
