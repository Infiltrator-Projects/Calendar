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
    (
'''    const extensionModules = {
        calendar: {}, eventManager: {}, eventView: {},
        panelClock: { CLOCK_MODE_STANDARD: "standard" },
    };
''',
'''    const extensionModules = {
        calendar: {}, eventManager: {}, eventView: {},
        nativeBoundary: {
            NativeBoundary: {
                decodeTemporalPolicy(variant) {
                    const row = variant.deep_unpack();
                    return {
                        mode: row[0],
                        calendar: row[1],
                        showSeconds: row[2],
                        locationConfigured: row[3],
                        latitude: row[4],
                        longitude: row[5],
                        authority: row[6],
                        providerAvailable: row[7],
                    };
                },
            },
        },
        panelClock: { CLOCK_MODE_STANDARD: "standard" },
    };
'''
    ),
    (
'''                        if (moduleName === "eventRangeState") {
                            return context.__eventRangeStateModule;
                        }
                        throw new Error(`unexpected local module ${moduleName}`);
''',
'''                        if (moduleName === "eventRangeState") {
                            return context.__eventRangeStateModule;
                        }
                        if (moduleName === "nativeBoundary") {
                            return {
                                NativeBoundary: {
                                    decodeEventSnapshot(variant) {
                                        const [revision, rows] = variant.deep_unpack();
                                        return {
                                            revision,
                                            events: rows.map((row) => ({
                                                id: row[0], color: row[1],
                                                summary: row[2], allDay: row[3],
                                                multiDay: row[4], startUnix: row[5],
                                                endUnix: row[6], startDayUnix: row[7],
                                                endDayUnix: row[8], modified: row[9],
                                            })),
                                        };
                                    },
                                },
                            };
                        }
                        throw new Error(`unexpected local module ${moduleName}`);
'''
    ),
    (
'''                                add_or_update(event, timestamp) {
                                    observations.addOrUpdateCalls.push(
                                        [event, timestamp]
                                    );
                                    return true;
                                },
''',
'''                                add_or_update(event, timestamp) {
                                    observations.addOrUpdateCalls.push(
                                        [event, timestamp]
                                    );
                                    return true;
                                },
                                add_or_update_in_range(event, timestamp) {
                                    observations.addOrUpdateCalls.push(
                                        [event, timestamp]
                                    );
                                    return true;
                                },
'''
    ),
    (
'''            if (name === "./runtimeSupport") {
                return { SignalBag: class SignalBag {
''',
'''            if (name === "./runtimeSupport") {
                return {
                    loadLocalModule(moduleName) {
                        if (moduleName !== "nativeBoundary") {
                            throw new Error(`unexpected local module ${moduleName}`);
                        }
                        return {
                            NativeBoundary: {
                                decodeCivilDate(variant) {
                                    const row = variant.deep_unpack();
                                    return { year: row[0], month: row[1], day: row[2] };
                                },
                                decodeCalendarGrid(variant) {
                                    return variant.deep_unpack().map((row) => ({
                                        dayLabel: row[0], year: row[1], month: row[2],
                                        day: row[3], row: row[4], column: row[5],
                                        weekNumber: row[6], isWorkDay: row[7],
                                        isToday: row[8], isSelected: row[9],
                                        isCurrentPeriod: row[10], isTopRow: row[11],
                                        isLeftEdge: row[12],
                                    }));
                                },
                            },
                        };
                    },
                    SignalBag: class SignalBag {
'''
    ),
    (
'''                }};
            }
            throw new Error(`unexpected module ${name}`);
''',
'''                }};
            }
            throw new Error(`unexpected module ${name}`);
'''
    ),
]

for old, new in replacements:
    count = source.count(old)
    assert count == 1, f"expected one runtime-test contract block, found {count}"
    source = source.replace(old, new, 1)

old_constructor = "new Calendar(settings, eventsManager, desktopSettings)"
count = source.count(old_constructor)
assert count >= 1, "expected Calendar harness constructor calls"
source = source.replace(old_constructor, "new Calendar(eventsManager, 0, false)")

path.write_text(source, encoding="utf-8")
