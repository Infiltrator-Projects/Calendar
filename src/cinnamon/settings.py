#!/usr/bin/python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith
"""Calendar design host for Cinnamon's native xlet settings UI.

Cinnamon's generic xlet-settings process is a separate GTK application, so it
cannot inherit the St/Cinnamon stylesheet used by the panel applet. This thin
host deliberately reuses Cinnamon's own MainWindow, JSON settings widgets,
persistence, D-Bus callbacks, import/export and reset behaviour while projecting
the same Common-owned typography and System/Day/Night appearance contract as
the applet itself.
"""

from __future__ import annotations

import argparse
import importlib.util
import signal
import sys
from pathlib import Path

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, Gio, Gtk  # noqa: E402

UUID = "calendar-plus@the-infiltratr"
CINNAMON_SETTINGS_DIR = Path("/usr/share/cinnamon/cinnamon-settings")
XLET_SETTINGS = CINNAMON_SETTINGS_DIR / "xlet-settings.py"

# BEGIN GENERATED COMMON TYPOGRAPHY TOKENS
SETTINGS_PAGE_MARGIN = 20
SETTINGS_SECTION_SPACING = 18
SETTINGS_CONTROL_SPACING = 10

CSS = b"""
* {
    font-family: "MB Corpo S Title WEB";
    font-weight: 400;
}
headerbar .title {
    font-family: "MB Corpo A Title Cond WEB";
    font-weight: 400;
}
button, button label {
    font-family: "MB Corpo S Title WEB";
    font-weight: 700;
}
button,
combobox button,
entry,
spinbutton,
switch {
    border-radius: 10px;
}
switch slider {
    border-radius: 6px;
}
frame.view,
tooltip {
    border-radius: 12px;
}
"""
THEME_CSS = {
    "day": b"""
window,
window.background,
.background,
viewport,
scrolledwindow {
    background-color: #FFFFFF;
    color: #20252B;
}
toolbar.primary-toolbar {
    background-image: none;
    background-color: #FFFFFF;
    color: #20252B;
    border-bottom: 1px solid #C7CDD3;
}
toolbar.primary-toolbar button {
    background-image: none;
    background-color: #ECEFF2;
    color: #111418;
    border: 1px solid #C7CDD3;
    box-shadow: none;
}
toolbar.primary-toolbar button:hover {
    background-color: #EEF1F3;
    border-color: #00ADEF;
}
frame.view {
    background-image: linear-gradient(to bottom right, #F8F9FA, #ECEFF2);
    color: #20252B;
    border: 1px solid #C7CDD3;
}
frame.view > border {
    border-color: #C7CDD3;
}
frame.view list,
frame.view row {
    background-color: transparent;
    color: #20252B;
}
frame.view row:hover {
    background-color: #EEF1F3;
}
frame.view label {
    color: #20252B;
}
separator {
    background-color: #C7CDD3;
    color: #C7CDD3;
    min-height: 1px;
}
entry,
spinbutton {
    background-image: none;
    background-color: #FFFFFF;
    color: #20252B;
    border: 1px solid #C7CDD3;
    box-shadow: none;
}
button,
combobox button {
    background-image: none;
    background-color: #E8ECEF;
    color: #20252B;
    border: 1px solid #C7CDD3;
    box-shadow: none;
}
button label,
button image,
combobox button label,
combobox button image {
    color: #20252B;
}
button:hover,
combobox button:hover {
    background-color: #DDE2E7;
    border-color: #00ADEF;
}
button:active,
button:checked {
    background-color: #DDE2E7;
    color: #111418;
    border-color: #00ADEF;
}
button:disabled,
combobox button:disabled {
    background-color: #FFFFFF;
    color: #737D86;
    border-color: #C7CDD3;
}
switch {
    background-image: none;
    background-color: #ECEFF2;
    border: 1px solid #C7CDD3;
    box-shadow: none;
}
switch slider {
    background-image: none;
    background-color: #20252B;
    border: 1px solid #C7CDD3;
    box-shadow: none;
}
switch:checked {
    background-color: #00ADEF;
    border-color: #00ADEF;
}
switch:checked slider {
    background-color: #031018;
    border-color: #25B8F0;
}
row:selected,
treeview.view:selected {
    background-color: #DDE2E7;
    color: #111418;
}
menu {
    background-color: #FFFFFF;
    color: #20252B;
    border: 1px solid #C7CDD3;
}
menuitem:hover {
    background-color: #F1F3F5;
}
tooltip {
    background-color: #F8F9FA;
    color: #111418;
    border: 1px solid #C7CDD3;
}
tooltip * {
    color: #111418;
}
.dim-label {
    color: #59636C;
}
label:disabled {
    color: #737D86;
}
""",
    "night": b"""
window,
window.background,
.background,
viewport,
scrolledwindow {
    background-color: #050608;
    color: #E8ECEF;
}
toolbar.primary-toolbar {
    background-image: none;
    background-color: #101318;
    color: #E8ECEF;
    border-bottom: 1px solid #353A40;
}
toolbar.primary-toolbar button {
    background-image: none;
    background-color: #0D1014;
    color: #EEF1F3;
    border: 1px solid #353A40;
    box-shadow: none;
}
toolbar.primary-toolbar button:hover {
    background-color: #22272D;
    border-color: #00ADEF;
}
frame.view {
    background-image: linear-gradient(to bottom right, #171B20, #0D1014);
    color: #E8ECEF;
    border: 1px solid #353A40;
}
frame.view > border {
    border-color: #353A40;
}
frame.view list,
frame.view row {
    background-color: transparent;
    color: #E8ECEF;
}
frame.view row:hover {
    background-color: #22272D;
}
frame.view label {
    color: #E8ECEF;
}
separator {
    background-color: #353A40;
    color: #353A40;
    min-height: 1px;
}
entry,
spinbutton {
    background-image: none;
    background-color: #0E1115;
    color: #E8ECEF;
    border: 1px solid #31363B;
    box-shadow: none;
}
button,
combobox button {
    background-image: none;
    background-color: #20252B;
    color: #E8ECEF;
    border: 1px solid #353A40;
    box-shadow: none;
}
button label,
button image,
combobox button label,
combobox button image {
    color: #E8ECEF;
}
button:hover,
combobox button:hover {
    background-color: #2B3137;
    border-color: #00ADEF;
}
button:active,
button:checked {
    background-color: #2B3137;
    color: #EEF1F3;
    border-color: #00ADEF;
}
button:disabled,
combobox button:disabled {
    background-color: #0E1115;
    color: #899198;
    border-color: #353A40;
}
switch {
    background-image: none;
    background-color: #0D1014;
    border: 1px solid #3B4147;
    box-shadow: none;
}
switch slider {
    background-image: none;
    background-color: #D7DDE2;
    border: 1px solid #31363B;
    box-shadow: none;
}
switch:checked {
    background-color: #00ADEF;
    border-color: #00ADEF;
}
switch:checked slider {
    background-color: #031018;
    border-color: #25B8F0;
}
row:selected,
treeview.view:selected {
    background-color: #2B3137;
    color: #EEF1F3;
}
menu {
    background-color: #101318;
    color: #E8ECEF;
    border: 1px solid #353A40;
}
menuitem:hover {
    background-color: #171B20;
}
tooltip {
    background-color: #171B20;
    color: #EEF1F3;
    border: 1px solid #353A40;
}
tooltip * {
    color: #EEF1F3;
}
.dim-label {
    color: #98A1A9;
}
label:disabled {
    color: #899198;
}
""",
}
# END GENERATED COMMON TYPOGRAPHY TOKENS


def tune_calendar_layout(widget) -> None:
    """Apply Common structural rhythm to Cinnamon's native settings widgets."""
    widget_type = widget.__class__.__name__
    if widget_type == "SettingsPage":
        widget.set_margin_left(SETTINGS_PAGE_MARGIN)
        widget.set_margin_right(SETTINGS_PAGE_MARGIN)
        widget.set_margin_top(SETTINGS_PAGE_MARGIN)
        widget.set_margin_bottom(SETTINGS_PAGE_MARGIN)
        widget.set_spacing(SETTINGS_SECTION_SPACING)
    elif widget_type == "SettingsSection":
        widget.set_spacing(SETTINGS_CONTROL_SPACING)

    if isinstance(widget, Gtk.Container):
        for child in widget.get_children():
            tune_calendar_layout(child)


def load_cinnamon_settings():
    if not XLET_SETTINGS.is_file():
        raise SystemExit(f"Cinnamon xlet settings renderer not found: {XLET_SETTINGS}")

    path = str(CINNAMON_SETTINGS_DIR)
    if path not in sys.path:
        sys.path.insert(0, path)

    spec = importlib.util.spec_from_file_location(
        "calendar_plus_cinnamon_xlet_settings",
        XLET_SETTINGS,
    )
    if spec is None or spec.loader is None:
        raise SystemExit("Unable to load Cinnamon xlet settings renderer")

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def install_calendar_style(window) -> None:
    screen = Gdk.Screen.get_default()
    if screen is None:
        return

    provider = Gtk.CssProvider()
    desktop = Gio.Settings.new("org.cinnamon.desktop.interface")

    def system_prefers_dark() -> bool:
        try:
            theme = desktop.get_string("gtk-theme")
        except Exception:
            return False
        return isinstance(theme, str) and "dark" in theme.lower()

    def effective_theme() -> str:
        mode = "system"
        try:
            if window.selected_instance is not None:
                mode = window.selected_instance["settings"].get_value("theme-mode")
        except (KeyError, TypeError):
            mode = "system"
        if mode not in ("system", "day", "night"):
            mode = "system"
        if mode == "system":
            return "night" if system_prefers_dark() else "day"
        return mode

    def refresh(*_args) -> None:
        provider.load_from_data(CSS + THEME_CSS[effective_theme()])

    Gtk.StyleContext.add_provider_for_screen(
        screen,
        provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
    )

    for info in window.instance_info:
        settings = info.get("settings")
        if settings is not None and settings.has_key("theme-mode"):
            settings.listen("theme-mode", refresh)

    window.instance_stack.connect("notify::visible-child-name", refresh)
    desktop.connect("changed::gtk-theme", refresh)
    refresh()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Calendar settings")
    parser.add_argument(
        "--instance",
        type=int,
        default=None,
        help="Calendar applet instance to configure",
    )
    parser.add_argument(
        "--tab",
        type=int,
        default=None,
        help="Settings tab index",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    cinnamon_settings = load_cinnamon_settings()

    native_args = argparse.Namespace(
        type="applet",
        uuid=UUID,
        id=args.instance,
        tab=args.tab,
    )
    window = cinnamon_settings.MainWindow(native_args)
    tune_calendar_layout(window.window)
    install_calendar_style(window)
    signal.signal(signal.SIGINT, window.quit)
    Gtk.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
