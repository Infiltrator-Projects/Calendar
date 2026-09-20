#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

"""Validate and deterministically format Calendar's Cinnamon settings schema."""

from __future__ import annotations

import json
import sys
from collections import OrderedDict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "src/cinnamon/settings-schema.json"

FORBIDDEN_TEMPORAL_KEYS = {
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
    "format-button",
}


def generated_schema() -> str:
    schema = json.loads(
        SCHEMA.read_text(encoding="utf-8"),
        object_pairs_hook=OrderedDict,
    )
    leaked = sorted(FORBIDDEN_TEMPORAL_KEYS.intersection(schema))
    if leaked:
        raise RuntimeError(
            "Calendar must not own temporal presentation settings; "
            f"move these to System Settings: {leaked}"
        )
    return json.dumps(schema, ensure_ascii=False, indent=4) + "\n"


def main() -> None:
    check = sys.argv[1:] == ["--check"]
    if sys.argv[1:] not in ([], ["--check"]):
        raise SystemExit("usage: update-settings.py [--check]")

    output = generated_schema()
    current = SCHEMA.read_text(encoding="utf-8")
    if check:
        if current != output:
            raise SystemExit(
                "settings-schema.json is not deterministically formatted; "
                "run tools/update-settings.py"
            )
        return

    SCHEMA.write_text(output, encoding="utf-8")


if __name__ == "__main__":
    main()
