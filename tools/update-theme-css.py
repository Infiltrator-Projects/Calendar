#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate Calendar theme colours from the pinned Common design contract.

Selectors and Cinnamon widget mechanics remain local to Calendar. Common
owns only semantic theme values. This keeps the CSS native to Cinnamon without
allowing a second private palette truth to drift from Common.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON_DESIGN = ROOT / "src/vendor/infiltratr-common/design/infiltrator-design-v1.json"
STYLESHEET = ROOT / "src/cinnamon/stylesheet.css"
BEGIN = "/* BEGIN GENERATED COMMON THEME TOKENS */"
END = "/* END GENERATED COMMON THEME TOKENS */"


def palette(data: dict, mode: str) -> dict[str, str]:
    return data["theme"]["palettes"][mode]


def render(data: dict) -> str:
    day = palette(data, "day")
    night = palette(data, "night")
    return f"""{BEGIN}
/*
 * Generated from Infiltratr Common. Do not hand-edit colour values here.
 * Calendar deliberately owns only Cinnamon selectors/widget mechanics.
 */
.calendar-plus-popup.calendar-plus-theme-night,
.calendar-plus-popup.calendar-plus-theme-night .calendar-main-box {{
    background-color: {night["background"]};
    color: {night["text"]};
}}

.calendar-plus-popup.calendar-plus-theme-night .calendar-today-home-button,
.calendar-plus-popup.calendar-plus-theme-night .calendar-today-home-button-enabled,
.calendar-plus-popup.calendar-plus-theme-night .calendar,
.calendar-plus-popup.calendar-plus-theme-night .calendar-events-main-box {{
    background-color: {night["panel"]};
    color: {night["text"]};
}}

.calendar-plus-popup.calendar-plus-theme-night .calendar-day-base,
.calendar-plus-popup.calendar-plus-theme-night .calendar-day-heading,
.calendar-plus-popup.calendar-plus-theme-night .calendar-month-label,
.calendar-plus-popup.calendar-plus-theme-night .calendar-events-date-label,
.calendar-plus-popup.calendar-plus-theme-night .calendar-events-no-events-label {{
    color: {night["title"]};
}}

.calendar-plus-popup.calendar-plus-theme-night .calendar-day-base:hover,
.calendar-plus-popup.calendar-plus-theme-night .calendar-today-home-button-enabled:hover {{
    background-color: {night["surface_hover"]};
}}

.calendar-plus-popup.calendar-plus-theme-day,
.calendar-plus-popup.calendar-plus-theme-day .calendar-main-box {{
    background-color: {day["background"]};
    color: {day["text"]};
}}

.calendar-plus-popup.calendar-plus-theme-day .calendar-today-home-button,
.calendar-plus-popup.calendar-plus-theme-day .calendar-today-home-button-enabled,
.calendar-plus-popup.calendar-plus-theme-day .calendar,
.calendar-plus-popup.calendar-plus-theme-day .calendar-events-main-box {{
    background-color: {day["panel"]};
    color: {day["text"]};
}}

.calendar-plus-popup.calendar-plus-theme-day .calendar-day-base,
.calendar-plus-popup.calendar-plus-theme-day .calendar-day-heading,
.calendar-plus-popup.calendar-plus-theme-day .calendar-month-label,
.calendar-plus-popup.calendar-plus-theme-day .calendar-events-date-label,
.calendar-plus-popup.calendar-plus-theme-day .calendar-events-no-events-label {{
    color: {day["title"]};
}}

.calendar-plus-popup.calendar-plus-theme-day .calendar-day-base:hover,
.calendar-plus-popup.calendar-plus-theme-day .calendar-today-home-button-enabled:hover {{
    background-color: {day["surface_hover"]};
}}

.calendar-plus-popup.calendar-plus-theme-day .calendar-day-base.calendar-day-selected,
.calendar-plus-popup.calendar-plus-theme-day .calendar-day-base.calendar-today {{
    background-color: {day["selection_background"]};
    color: {day["selection_foreground"]};
}}

.calendar-plus-popup.calendar-plus-theme-night .calendar-day-base.calendar-day-selected,
.calendar-plus-popup.calendar-plus-theme-night .calendar-day-base.calendar-today {{
    background-color: {night["selection_background"]};
    color: {night["selection_foreground"]};
}}
{END}"""


def desired_stylesheet() -> str:
    data = json.loads(COMMON_DESIGN.read_text(encoding="utf-8"))
    source = STYLESHEET.read_text(encoding="utf-8")
    generated = render(data)

    if BEGIN in source and END in source:
        before, rest = source.split(BEGIN, 1)
        _, after = rest.split(END, 1)
        return before.rstrip() + "\n\n" + generated + after

    marker = "/*\n * Theme policy\n"
    if marker not in source:
        raise SystemExit("Calendar theme policy marker not found")
    before = source.split(marker, 1)[0].rstrip()
    return before + "\n\n/*\n * Theme policy\n * ------------\n * Follow system is platform-authoritative; Day/Night values come from Common.\n */\n\n" + generated + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    desired = desired_stylesheet()
    if args.check:
        actual = STYLESHEET.read_text(encoding="utf-8")
        if actual != desired:
            raise SystemExit("Calendar theme CSS is stale; run make update-theme")
        return 0
    STYLESHEET.write_text(desired, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
