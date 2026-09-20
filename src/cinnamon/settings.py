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
button {
    border-radius: 10px;
}
"""
THEME_CSS = {
    "day": b"""
window,
window.background,
.background,
.view,
viewport {
    background-color: #FFFFFF;
    color: #20252B;
}
headerbar {
    background-color: #ECEFF2;
    color: #111418;
    border-color: #C7CDD3;
}
headerbar label,
headerbar .title {
    color: #111418;
}
frame > border,
separator {
    border-color: #C7CDD3;
    background-color: #C7CDD3;
}
entry,
spinbutton,
combobox button {
    background-color: #FFFFFF;
    color: #20252B;
    border-color: #C7CDD3;
}
button {
    background-color: #E8ECEF;
    color: #20252B;
    border-color: #C7CDD3;
}
button:hover {
    background-color: #DDE2E7;
    border-color: #00ADEF;
}
switch {
    background-color: #ECEFF2;
    border-color: #C7CDD3;
}
switch:checked {
    background-color: #00ADEF;
    color: #031018;
}
row:selected,
treeview.view:selected {
    background-color: #DDE2E7;
    color: #111418;
}
.dim-label {
    color: #59636C;
}
""",
    "night": b"""
window,
window.background,
.background,
.view,
viewport {
    background-color: #050608;
    color: #E8ECEF;
}
headerbar {
    background-color: #202125;
    color: #E7EBEE;
    border-color: #353A40;
}
headerbar label,
headerbar .title {
    color: #E7EBEE;
}
frame > border,
separator {
    border-color: #353A40;
    background-color: #353A40;
}
entry,
spinbutton,
combobox button {
    background-color: #0E1115;
    color: #E8ECEF;
    border-color: #31363B;
}
button {
    background-color: #20252B;
    color: #E8ECEF;
    border-color: #353A40;
}
button:hover {
    background-color: #2B3137;
    border-color: #00ADEF;
}
switch {
    background-color: #0D1014;
    border-color: #353A40;
}
switch:checked {
    background-color: #00ADEF;
    color: #031018;
}
row:selected,
treeview.view:selected {
    background-color: #2B3137;
    color: #EEF1F3;
}
.dim-label {
    color: #98A1A9;
}
""",
}
# END GENERATED COMMON TYPOGRAPHY TOKENS


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

    def system_theme_name() -> str:
        try:
            theme = desktop.get_string("gtk-theme")
        except Exception:
            return ""
        return theme.lower() if isinstance(theme, str) else ""

    def system_prefers_dark() -> bool:
        return "dark" in system_theme_name()

    def system_uses_high_contrast() -> bool:
        compact = system_theme_name().replace("-", "").replace("_", "").replace(" ", "")
        return "highcontrast" in compact

    def effective_theme():
        mode = "system"
        try:
            if window.selected_instance is not None:
                mode = window.selected_instance["settings"].get_value("theme-mode")
        except (KeyError, TypeError):
            mode = "system"
        if mode not in ("system", "day", "night"):
            mode = "system"
        if mode == "system":
            if system_uses_high_contrast():
                return None
            return "night" if system_prefers_dark() else "day"
        return mode

    def refresh(*_args) -> None:
        theme = effective_theme()
        provider.load_from_data(CSS if theme is None else CSS + THEME_CSS[theme])

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
    install_calendar_style(window)
    signal.signal(signal.SIGINT, window.quit)
    Gtk.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
