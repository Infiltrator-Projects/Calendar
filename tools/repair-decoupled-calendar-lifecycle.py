#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test-js-runtime.js"
source = path.read_text(encoding="utf-8")
old = '''    assert.equal(
        observations.desktopDisconnects,
        1,
        "calendar must disconnect its desktop-settings signal"
    );
'''
new = '''    assert.equal(
        observations.desktopDisconnects,
        0,
        "calendar must not own desktop-settings signals below the composition root"
    );
'''
assert source.count(old) == 1
path.write_text(source.replace(old, new, 1), encoding="utf-8")
