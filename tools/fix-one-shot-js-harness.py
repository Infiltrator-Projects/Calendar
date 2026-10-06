#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Correct the temporary Node harness so GJS modules keep separate scopes."""

from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test-js-runtime.js"
text = path.read_text(encoding="utf-8")
old = '''    vm.runInContext(
        `${rangeStateSource}\\nglobalThis.__EventRangeState = EventRangeState;`,
        context,
        { filename: "eventRangeState.js" }
    );
    context.__eventRangeStateModule = {
        EventRangeState: context.__EventRangeState,
    };
'''
new = '''    context.__eventRangeStateModule = vm.runInContext(
        `(() => {\\n${rangeStateSource}\\nreturn { EventRangeState };\\n})()`,
        context,
        { filename: "eventRangeState.js" }
    );
'''
count = text.count(old)
if count != 1:
    raise SystemExit(f"expected one generated EventRangeState harness, found {count}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
print("Node module-scope harness corrected")
