#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test-js-runtime.js"
source = path.read_text(encoding="utf-8")

replacements = [
    (
'''        vertical: false,
        desktopSettings: {
            get_boolean(key) { return key === "clock-use-24h"; },
        },
        primaryCalendar: "gregorian",
''',
'''        vertical: false,
        use24h: true,
        showDate: false,
        primaryCalendar: "gregorian",
'''
    ),
    (
'''        primaryCalendar: "positivist",
        desktopSettings: {
            get_boolean(key) {
                if (key === "clock-use-24h") return false;
                if (key === "clock-show-date") return true;
                throw new Error(`unexpected Cinnamon key: ${key}`);
            },
        },
''',
'''        primaryCalendar: "positivist",
        use24h: false,
        showDate: true,
'''
    ),
    (
'''    const settings = {
        get_boolean(key) {
            return key === "clock-use-24h" || key === "clock-show-date";
        },
    };
''',
''
    ),
    (
'''        vertical: false,
        desktopSettings: settings,
        primaryCalendar: "gregorian",
''',
'''        vertical: false,
        use24h: true,
        showDate: true,
        primaryCalendar: "gregorian",
'''
    ),
    (
'''        const desktopSettings = {
            get_boolean(key) {
                if (key === "clock-use-24h") return use24h;
                if (key === "clock-show-date") return showDate;
                throw new Error(`unexpected Cinnamon key: ${key}`);
            },
        };
        PanelClock.configureWallClock(clock, {
            mode,
            showSeconds,
            vertical,
            primaryCalendar,
            desktopSettings,
        });
''',
'''        PanelClock.configureWallClock(clock, {
            mode,
            showSeconds,
            vertical,
            primaryCalendar,
            use24h,
            showDate,
        });
'''
    ),
]

for old, new in replacements:
    count = source.count(old)
    assert count == 1, f"expected one runtime-test contract block, found {count}"
    source = source.replace(old, new, 1)

path.write_text(source, encoding="utf-8")
