#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import json
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT = Path(__file__).resolve().parents[1]
OLD = "1.0.82"
NEW = "1.0.83"


def replace_once(path: str, old: str, new: str) -> None:
    file_path = ROOT / path
    text = file_path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise SystemExit(f"{path}: expected exactly one guarded occurrence of {old!r}")
    file_path.write_text(text.replace(old, new, 1), encoding="utf-8")


replace_once("Makefile", f"VERSION := {OLD}", f"VERSION := {NEW}")

metadata_path = ROOT / "src/cinnamon/metadata.json"
metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
if metadata.get("version") != OLD:
    raise SystemExit("src/cinnamon/metadata.json: unexpected source version")
metadata["version"] = NEW
metadata_path.write_text(json.dumps(metadata, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")

changelog_path = ROOT / "CHANGELOG.md"
changelog = changelog_path.read_text(encoding="utf-8")
anchor = f"## Unreleased\n\nNo unreleased changes.\n\n## {OLD} - 2026-10-06"
entry = (
    "## Unreleased\n\nNo unreleased changes.\n\n"
    f"## {NEW} - 2026-10-07\n\n"
    "- Isolate CalendarServer visible-range request lifecycle state behind `EventRangeState`, so generation, admission, queued-force and retry transitions have one owner.\n"
    "- Route civil-day navigation and weekday calculation through native CalendarPlus APIs, removing duplicate JavaScript chronology logic.\n"
    "- Clarify event snapshot identity as an opaque `revision` rather than a timestamp and strengthen architecture/runtime regression coverage around these boundaries.\n\n"
    f"## {OLD} - 2026-10-06"
)
if changelog.count(anchor) != 1:
    raise SystemExit("CHANGELOG.md: release anchor changed")
changelog_path.write_text(changelog.replace(anchor, entry, 1), encoding="utf-8")

debian_path = ROOT / "debian/changelog"
debian = debian_path.read_text(encoding="utf-8")
if not debian.startswith(f"infiltrator-calendar ({OLD}) "):
    raise SystemExit("debian/changelog: unexpected current release")
now = datetime.now(ZoneInfo("Australia/Brisbane"))
stamp = now.strftime("%a, %d %b %Y %H:%M:%S %z")
debian_entry = (
    f"infiltrator-calendar ({NEW}) unstable; urgency=medium\n\n"
    "  * Isolate CalendarServer visible-range request lifecycle state behind\n"
    "    EventRangeState so request transitions have one owner.\n"
    "  * Route civil-day navigation and weekday calculation through native\n"
    "    CalendarPlus APIs instead of duplicate JavaScript chronology.\n"
    "  * Rename event snapshot identity to revision and strengthen regression\n"
    "    coverage around the refactored boundaries.\n\n"
    f" -- Shannon Smith <noreply@the-infiltratr.com>  {stamp}\n\n"
)
debian_path.write_text(debian_entry + debian, encoding="utf-8")
