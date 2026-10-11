#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test-js-runtime.js"
source = path.read_text(encoding="utf-8")
old = "assert.equal(LegacyAppletClass.loaderObservations.featureCalls, 4);"
new = "assert.equal(LegacyAppletClass.loaderObservations.featureCalls, 5);"
assert source.count(old) == 1
source = source.replace(old, new, 1)
old = "assert.equal(ExtensionAppletClass.loaderObservations.featureCalls, 4);"
new = "assert.equal(ExtensionAppletClass.loaderObservations.featureCalls, 5);"
assert source.count(old) == 1
source = source.replace(old, new, 1)
path.write_text(source, encoding="utf-8")
