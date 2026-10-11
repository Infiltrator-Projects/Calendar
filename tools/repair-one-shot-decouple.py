#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).with_name("one-shot-decouple-boundaries.py")
source = path.read_text(encoding="utf-8")
old = '''replace_once(
    "src/cinnamon/calendar.js",
    ''' + "'''" + '''                    record[1], record[2], record[3]\\n''' + "'''" + ''',
    ''' + "'''" + '''                    record.year, record.month, record.day\\n''' + "'''" + '''
)
# There are two endpoint loops using the same positional expression.
'''
new = '''# Both endpoint loops intentionally use the same native grid-record fields.
'''
count = source.count(old)
assert count == 1, f"expected one transform block, found {count}"
path.write_text(source.replace(old, new, 1), encoding="utf-8")
