#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).with_name("repair-decoupled-js-tests.py")
source = path.read_text(encoding="utf-8")
block = '''    (\n''' + "'''" + '''                }};\n            }\n            throw new Error(`unexpected module ${name}`);\n''' + "''',\n'''" + '''                }};\n            }\n            throw new Error(`unexpected module ${name}`);\n''' + "'''" + '''\n    ),\n'''
count = source.count(block)
assert count == 1, f"expected one redundant transform, found {count}"
path.write_text(source.replace(block, "", 1), encoding="utf-8")
