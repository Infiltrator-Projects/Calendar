#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Project Common design tokens into Calendar's Cinnamon and GTK surfaces.

Calendar owns Cinnamon/GTK selectors and widget mechanics. Infiltratr Common
owns semantic theme values and typography identity. This generator keeps the
toolkit-specific source native while preventing a second private design truth
from drifting away from the pinned Common release.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON_DESIGN = ROOT / "src/vendor/infiltratr-common/design/infiltrator-design-v1.json"
STYLESHEET = ROOT / "src/cinnamon/stylesheet.css"
SETTINGS = ROOT / "src/cinnamon/settings.py"

TYPOGRAPHY_BEGIN = "/* BEGIN GENERATED COMMON TYPOGRAPHY TOKENS */"
TYPOGRAPHY_END = "/* END GENERATED COMMON TYPOGRAPHY TOKENS */"
SETTINGS_TYPOGRAPHY_BEGIN = "# BEGIN GENERATED COMMON TYPOGRAPHY TOKENS"
SETTINGS_TYPOGRAPHY_END = "# END GENERATED COMMON TYPOGRAPHY TOKENS"
THEME_BEGIN = "/* BEGIN GENERATED COMMON THEME TOKENS */"
THEME_END = "/* END GENERATED COMMON THEME TOKENS */"


def palette(data: dict, mode: str) -> dict[str, str]:
    return data["theme"]["palettes"][mode]


def typography(data: dict) -> dict:
    return data["typography"]


def render_typography(data: dict) -> str:
    type_data = typography(data)
    ui = type_data["ui_family"]
    regular = type_data["ui_regular_weight"]
    bold = type_data["ui_bold_weight"]
    return f"""{TYPOGRAPHY_BEGIN}
/*
 * Generated from Infiltratr Common. Do not hand-edit typography values here.
 * Calendar deliberately owns only Cinnamon selectors/widget mechanics.
 */
.calendar-plus-applet,
.calendar-plus-applet *,
.calendar-plus-popup,
.calendar-plus-popup * {{
    font-family: "{ui}";
    font-weight: {regular};
}}

.calendar-plus-panel-clock {{
    font-family: "{ui}";
    font-weight: {regular};
    font-size: 1.08em;
}}

.calendar-plus-popup .calendar-today-day-label,
.calendar-plus-popup .calendar-month-label,
.calendar-plus-popup .calendar-day-heading,
.calendar-plus-popup .calendar-events-date-label,
.calendar-plus-popup .calendar-events-no-events-label {{
    font-family: "{ui}";
    font-weight: {bold};
}}
{TYPOGRAPHY_END}"""


def render_settings_typography(data: dict) -> str:
    type_data = typography(data)
    ui = type_data["ui_family"]
    brand = type_data["brand_family"]
    regular = type_data["ui_regular_weight"]
    bold = type_data["ui_bold_weight"]
    brand_weight = type_data["brand_weight"]
    return f'''{SETTINGS_TYPOGRAPHY_BEGIN}
CSS = b"""
* {{
    font-family: "{ui}";
    font-weight: {regular};
}}
headerbar .title {{
    font-family: "{brand}";
    font-weight: {brand_weight};
}}
button, button label {{
    font-family: "{ui}";
    font-weight: {bold};
}}
"""
{SETTINGS_TYPOGRAPHY_END}'''


def render_theme(data: dict) -> str:
    day = palette(data, "day")
    night = palette(data, "night")
    return f"""{THEME_BEGIN}
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
{THEME_END}"""


def replace_block(source: str, begin: str, end: str, generated: str, label: str) -> str:
    if begin not in source or end not in source:
        raise SystemExit(f"Calendar {label} generated markers are missing")
    before, rest = source.split(begin, 1)
    _, after = rest.split(end, 1)
    return before.rstrip() + "\n\n" + generated + after


def desired_stylesheet(data: dict) -> str:
    source = STYLESHEET.read_text(encoding="utf-8")
    source = replace_block(
        source,
        TYPOGRAPHY_BEGIN,
        TYPOGRAPHY_END,
        render_typography(data),
        "typography",
    )
    return replace_block(
        source,
        THEME_BEGIN,
        THEME_END,
        render_theme(data),
        "theme",
    )


def desired_settings(data: dict) -> str:
    source = SETTINGS.read_text(encoding="utf-8")
    return replace_block(
        source,
        SETTINGS_TYPOGRAPHY_BEGIN,
        SETTINGS_TYPOGRAPHY_END,
        render_settings_typography(data),
        "settings typography",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    data = json.loads(COMMON_DESIGN.read_text(encoding="utf-8"))
    desired_css = desired_stylesheet(data)
    desired_settings_source = desired_settings(data)

    if args.check:
        if STYLESHEET.read_text(encoding="utf-8") != desired_css:
            raise SystemExit("Calendar design CSS is stale; run make update-theme")
        if SETTINGS.read_text(encoding="utf-8") != desired_settings_source:
            raise SystemExit("Calendar settings typography is stale; run make update-theme")
        return 0

    STYLESHEET.write_text(desired_css, encoding="utf-8")
    SETTINGS.write_text(desired_settings_source, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
