#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2016-2026 Shannon Smith

"""Validate release inputs that Make cannot express safely.

This script is a build-time verifier only. It is never installed. It rejects
generated probes, caches and absolute workspace paths, enforcing release-input
hygiene before a source archive or Debian package is released.
Provider/settings drift is checked by tools/update-settings.py, keeping one parser
for the provider metadata rather than duplicating registry knowledge here.
"""

from __future__ import annotations

import binascii
import hashlib
import json
import os
import re
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APPLET = ROOT / "src/cinnamon"
PROJECT_URL = "https://github.com/Infiltrator-Projects/Calendar"
ICON = ROOT / "src/assets/infiltratr-calendar.png"
ICON_SHA256 = "92cace99117653bad9a89c23073f8aeabc1058b5e0ec61902dc8441eb13fa9ec"

TRANSIENT_PATTERNS = (
    re.compile(r"^g-ir-cpp-.*\.c$"),
    re.compile(r".*\.py[co]$"),
    re.compile(r"(^|/)__pycache__(/|$)"),
    re.compile(r"(^|/)\.package-root-"),
)



def validate_icon_asset() -> None:
    """Reject malformed PNG artwork before packaging or release."""
    data = ICON.read_bytes()
    assert hashlib.sha256(data).hexdigest() == ICON_SHA256, (
        "Calendar icon artwork does not match the canonical asset"
    )
    assert data.startswith(b"\x89PNG\r\n\x1a\n"), "Calendar icon is not a PNG"

    offset = 8
    saw_ihdr = False
    saw_iend = False
    idat = bytearray()

    while offset < len(data):
        assert offset + 12 <= len(data), "truncated PNG chunk header"
        length = struct.unpack(">I", data[offset:offset + 4])[0]
        chunk_type = data[offset + 4:offset + 8]
        chunk_start = offset + 8
        chunk_end = chunk_start + length
        crc_end = chunk_end + 4
        assert crc_end <= len(data), (
            f"truncated PNG {chunk_type.decode('ascii', errors='replace')} chunk"
        )

        chunk = data[chunk_start:chunk_end]
        stored_crc = struct.unpack(">I", data[chunk_end:crc_end])[0]
        calculated_crc = binascii.crc32(chunk_type)
        calculated_crc = binascii.crc32(chunk, calculated_crc) & 0xFFFFFFFF
        assert stored_crc == calculated_crc, (
            f"bad PNG CRC for {chunk_type.decode('ascii', errors='replace')}"
        )

        if chunk_type == b"IHDR":
            assert not saw_ihdr and length == 13, "invalid PNG IHDR"
            width, height, bit_depth, colour_type, compression, filter_method, interlace = (
                struct.unpack(">IIBBBBB", chunk)
            )
            assert (width, height) == (256, 256), "Calendar icon must be 256x256"
            assert bit_depth == 8, "Calendar icon must use 8-bit channels"
            assert colour_type in (2, 3, 6), "unsupported Calendar icon colour type"
            assert compression == 0 and filter_method == 0, "unsupported PNG encoding"
            assert interlace in (0, 1), "invalid PNG interlace method"
            saw_ihdr = True
        elif chunk_type == b"IDAT":
            idat.extend(chunk)
        elif chunk_type == b"IEND":
            assert length == 0, "invalid PNG IEND"
            saw_iend = True
            offset = crc_end
            break

        offset = crc_end

    assert saw_ihdr, "Calendar icon has no IHDR"
    assert idat, "Calendar icon has no IDAT payload"
    assert saw_iend, "Calendar icon has no IEND"
    assert offset == len(data), "Calendar icon has trailing bytes after IEND"
    try:
        zlib.decompress(bytes(idat))
    except zlib.error as exc:
        raise AssertionError("Calendar icon has invalid compressed image data") from exc


def validate_no_transient_files() -> None:
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith(("build/", "dist/")):
            continue
        assert not any(p.search(relative) for p in TRANSIENT_PATTERNS), (
            f"transient build file in release input: {relative}"
        )


def validate_no_workspace_paths() -> None:
    text_suffixes = {
        ".c",
        ".h",
        ".js",
        ".json",
        ".in",
        ".md",
        ".py",
        ".rules",
        ".sh",
        ".yml",
        ".yaml",
    }
    forbidden = (
        "/workspace/",
        "/tmp/",
        str(ROOT),
    )

    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix not in text_suffixes:
            continue
        if path == Path(__file__).resolve():
            continue
        if path.parts[-2:-1] in (("build",), ("dist",)):
            continue
        content = path.read_text(encoding="utf-8", errors="strict")
        for value in forbidden:
            assert value not in content, (
                f"absolute build path in {path.relative_to(ROOT)}: {value}"
            )


def validate_abi_manifest() -> None:
    version_map = (ROOT / "src/abi/calendar-plus.map").read_text(encoding="utf-8")
    mapped = set(re.findall(r"\b(calendar_plus_[a-z0-9_]+);", version_map))

    assert mapped, "linker version map exports no Calendar symbols"
    assert "CALENDAR_PLUS_1.0" in version_map
    assert "CALENDAR_PLUS_1.1" in version_map
    assert "CALENDAR_PLUS_1.2" in version_map
    assert re.search(
        r"CALENDAR_PLUS_1\.1\s*\{.*?\}\s*CALENDAR_PLUS_1\.0;",
        version_map,
        re.DOTALL,
    ), "native ABI 1.1 must inherit the 1.0 symbol set"
    assert re.search(
        r"CALENDAR_PLUS_1\.2\s*\{.*?\}\s*CALENDAR_PLUS_1\.1;",
        version_map,
        re.DOTALL,
    ), "native ABI 1.2 must inherit the 1.1 symbol set"


def validate_debian_source_hygiene() -> None:
    options_path = ROOT / "debian/source/options"
    assert options_path.is_file(), "missing debian/source/options"
    options = {
        line.strip()
        for line in options_path.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }
    assert "tar-ignore = build" in options, (
        "Debian source package must exclude the build directory"
    )
    assert "tar-ignore = dist" in options, (
        "Debian source package must exclude generated distribution archives"
    )
    assert "tar-ignore = __pycache__" in options, (
        "Debian source package must exclude Python bytecode caches"
    )
    assert "tar-ignore = *.pyc" in options, (
        "Debian source package must exclude compiled Python bytecode"
    )
    assert "tar-ignore = *.pyo" in options, (
        "Debian source package must exclude optimized Python bytecode"
    )


def validate_version() -> None:
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    changelog = (ROOT / "debian/changelog").read_text(encoding="utf-8")
    match = re.search(r"^VERSION := ([0-9]+\.[0-9]+\.[0-9]+)$",
                      makefile,
                      re.MULTILINE)
    assert match is not None
    version = match.group(1)
    build_mode = os.environ.get("CALENDAR_PLUS_BUILD_MODE", "generic")
    changelog_version_match = re.match(
        r"^infiltrator-calendar \(([^)]+)\) ", changelog
    )
    assert changelog_version_match is not None, "invalid Debian changelog"
    changelog_version = changelog_version_match.group(1)
    control = (ROOT / "debian/control").read_text(encoding="utf-8")
    assert re.search(r"^Source: infiltrator-calendar$", control, re.MULTILINE)
    assert re.search(r"^Package: infiltrator-calendar$", control, re.MULTILINE)
    assert "Breaks: calendar-plus (<< 1.0.17), cinnamon-calendar (<< 1.0.17)" in control
    assert "Replaces: calendar-plus (<< 1.0.17), cinnamon-calendar (<< 1.0.17)" in control
    if build_mode == "generic":
        assert changelog_version == version, (
            "generic Debian package and source versions differ"
        )
    elif build_mode == "native":
        assert re.fullmatch(
            rf"{re.escape(version)}\+native[1-9][0-9]*",
            changelog_version,
        ), "local native package must use <source-version>+nativeN"
    else:
        raise AssertionError(f"unsupported build mode: {build_mode}")

    metadata = json.loads(
        (APPLET / "metadata.json").read_text(encoding="utf-8")
    )
    assert metadata.get("version") == version, (
        "Makefile and applet metadata versions differ"
    )
    assert metadata.get("website") == PROJECT_URL, (
        "Cinnamon metadata does not use the canonical Calendar URL"
    )

    project_info = (ROOT / "src/app/project-info.c").read_text(encoding="utf-8")
    assert f'.website = "{PROJECT_URL}"' in project_info, (
        "native project metadata does not use the canonical Calendar URL"
    )
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert f"git clone --recurse-submodules {PROJECT_URL}.git" in readme, (
        "README clone instructions do not use the canonical Calendar URL"
    )

    applet_source = (APPLET / "applet.js").read_text(encoding="utf-8")
    assert "const APP_VERSION" not in applet_source, (
        "JavaScript must obtain its release identity from Cinnamon metadata"
    )
    assert "metadata.version" in applet_source, (
        "applet entry point does not pass metadata version to the ABI gate"
    )


def main() -> None:
    validate_icon_asset()
    validate_no_transient_files()
    validate_no_workspace_paths()
    validate_abi_manifest()
    validate_debian_source_hygiene()
    validate_version()


if __name__ == "__main__":
    main()
