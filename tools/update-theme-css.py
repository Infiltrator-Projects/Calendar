#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Project Common design tokens into Calendar's Cinnamon surface.

Calendar owns Cinnamon selectors and widget mechanics. Infiltratr Common owns
semantic theme values, typography identity and structural metrics. Common
1.19.23 carries the complete Linux MBLINK Night reference roles. This
generator keeps the toolkit-specific source native while preventing a second
private design truth from drifting away from the pinned Common release.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON_DESIGN = ROOT / "src/vendor/infiltratr-common/design/infiltrator-design-v1.json"
STYLESHEET = ROOT / "src/cinnamon/stylesheet.css"

TYPOGRAPHY_BEGIN = "/* BEGIN GENERATED COMMON TYPOGRAPHY TOKENS */"
TYPOGRAPHY_END = "/* END GENERATED COMMON TYPOGRAPHY TOKENS */"
THEME_BEGIN = "/* BEGIN GENERATED COMMON THEME TOKENS */"
THEME_END = "/* END GENERATED COMMON THEME TOKENS */"
METRICS_BEGIN = "/* BEGIN GENERATED COMMON METRIC TOKENS */"
METRICS_END = "/* END GENERATED COMMON METRIC TOKENS */"


def palette(data: dict, mode: str) -> dict[str, str]:
    return data["theme"]["palettes"][mode]


def typography(data: dict) -> dict:
    return data["typography"]


def metrics(data: dict) -> dict:
    return data["metrics"]


def render_typography(data: dict) -> str:
    type_data = typography(data)
    ui = type_data["ui_family"]
    brand = type_data["brand_family"]
    regular = type_data["ui_regular_weight"]
    bold = type_data["ui_bold_weight"]
    brand_weight = type_data["brand_weight"]
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
.calendar-plus-popup .calendar-day-heading,
.calendar-plus-popup .calendar-events-date-label,
.calendar-plus-popup .calendar-events-no-events-label {{
    font-family: "{ui}";
    font-weight: {bold};
}}

.calendar-plus-popup .calendar-month-label {{
    font-family: "{brand}";
    font-weight: {brand_weight};
}}
{TYPOGRAPHY_END}"""


def render_metrics(data: dict) -> str:
    metric_data = metrics(data)
    return f"""{METRICS_BEGIN}
/*
 * Generated from Infiltratr Common. Calendar keeps selectors local while
 * Common owns product-family structural rhythm and corner geometry.
 */
.calendar-plus-popup {{
    border-radius: {metric_data["panel_radius"]}px;
}}

.calendar-plus-popup .calendar-main-box {{
    spacing: {metric_data["section_spacing"]}px;
}}

.calendar-plus-popup .calendar,
.calendar-plus-popup .calendar-events-main-box {{
    border-radius: {metric_data["card_radius"]}px;
}}

.calendar-plus-popup .calendar-today-home-button,
.calendar-plus-popup .calendar-today-home-button-enabled {{
    border-radius: {metric_data["control_radius"]}px;
}}

.calendar-plus-popup .calendar-day-base {{
    border-radius: {metric_data["small_radius"]}px;
}}
{METRICS_END}"""

def render_theme(data: dict) -> str:
    day = palette(data, "day")
    night = palette(data, "night")

    def mode_css(mode: str, p: dict[str, str]) -> str:
        root = f".calendar-plus-popup.calendar-plus-theme-{mode}"
        return f"""
{root},
{root} .calendar-main-box {{
    background-color: {p["background"]};
    color: {p["text"]};
}}

{root} .calendar,
{root} .calendar-events-main-box {{
    background-color: {p["card"]};
    color: {p["text"]};
    border-color: {p["border"]};
}}

{root} .calendar-today-home-button,
{root} .calendar-today-home-button-enabled {{
    background-color: {p["surface"]};
    color: {p["heading"]};
    border-color: {p["status_border"]};
}}

{root} .calendar-month-label,
{root} .calendar-events-date-label,
{root} .calendar-events-no-events-label {{
    color: {p["heading"]};
}}

{root} .calendar-day-heading {{
    color: {p["summary"]};
}}

{root} .calendar-day-base {{
    color: {p["text"]};
}}

{root} .calendar-day-base:hover,
{root} .calendar-today-home-button-enabled:hover,
{root} .popup-menu-item:hover {{
    background-color: {p["surface_hover"]};
}}

{root} .calendar-day-base.calendar-day-selected,
{root} .calendar-day-base.calendar-today {{
    background-color: {p["neutral_accent"]};
    color: {p["accent_foreground"]};
}}

{root} .popup-menu-item {{
    color: {p["text"]};
}}

{root} .popup-separator-menu-item {{
    color: {p["border"]};
}}
"""

    return f"""{THEME_BEGIN}
/*
 * Generated from Infiltratr Common. Do not hand-edit colour values here.
 * Calendar deliberately owns only Cinnamon selectors/widget mechanics.
 * Common 1.19.24 carries the complete Linux MBLINK Night reference roles.
 */
{mode_css("night", night)}
{mode_css("day", day)}
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
    source = replace_block(
        source,
        METRICS_BEGIN,
        METRICS_END,
        render_metrics(data),
        "metrics",
    )
    return replace_block(
        source,
        THEME_BEGIN,
        THEME_END,
        render_theme(data),
        "theme",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    data = json.loads(COMMON_DESIGN.read_text(encoding="utf-8"))
    desired_css = desired_stylesheet(data)

    if args.check:
        if STYLESHEET.read_text(encoding="utf-8") != desired_css:
            raise SystemExit("Calendar design CSS is stale; run make update-theme")
        return 0

    STYLESHEET.write_text(desired_css, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
