#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith
"""Generate or verify Calendar's unavoidable pre-loader compatibility shim.

A Cinnamon module must locate runtimeSupport before it can call
runtimeSupport.loadLocalModule(), so this tiny bootstrap cannot itself live in
runtimeSupport without a circular dependency. Keep one canonical source here
and require every bootstrap-bearing module to contain it byte-for-byte.
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
APPLET = ROOT / "src/cinnamon"

CANONICAL = '''function _loadRuntimeSupport() {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports.runtimeSupport) {
                return extension.imports.runtimeSupport;
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }
    return require("./runtimeSupport");
}'''

FUNCTION_RE = re.compile(
    r'function _loadRuntimeSupport\(\) \{.*?\n\}',
    re.DOTALL,
)


def targets() -> list[Path]:
    return sorted(
        path
        for path in APPLET.glob("*.js")
        if path.name != "runtimeSupport.js" and
        "function _loadRuntimeSupport()" in path.read_text(encoding="utf-8")
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    paths = targets()
    if not paths:
        raise SystemExit("no Cinnamon runtime bootstrap users found")

    stale: list[str] = []
    for path in paths:
        source = path.read_text(encoding="utf-8")
        matches = list(FUNCTION_RE.finditer(source))
        if len(matches) != 1:
            raise SystemExit(
                f"{path.relative_to(ROOT)} must contain exactly one runtime bootstrap"
            )
        if matches[0].group(0) == CANONICAL:
            continue
        if args.check:
            stale.append(path.relative_to(ROOT).as_posix())
            continue
        path.write_text(
            source[:matches[0].start()] + CANONICAL + source[matches[0].end():],
            encoding="utf-8",
        )

    if stale:
        raise SystemExit(
            "Cinnamon runtime bootstrap drifted from its canonical generator: "
            + ", ".join(stale)
            + "; run python3 tools/update-runtime-loader.py"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
