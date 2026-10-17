#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot cleanup after the calendar resource migration."""

from pathlib import Path

path = Path("tests/test-release-model.py")
text = path.read_text(encoding="utf-8")
old = '    assert "for resource in hebrew gregorian persian coptic" in package_install\n'
new = '    assert "test -f /System/Resources/Calendars/hebrew.so" in package_install\n'
if text.count(old) != 1:
    raise SystemExit("expected migrated package-install assertion exactly once")
path.write_text(text.replace(old, new), encoding="utf-8")
