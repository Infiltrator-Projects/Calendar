#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

"use strict";

const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const root = path.resolve(__dirname, "..");
const metadata = JSON.parse(fs.readFileSync(
    path.join(root, "src", "cinnamon", "metadata.json"),
    "utf8"
));
const version = metadata.version;

function evaluateApplet(nativeVersion = version, loaderMode = "legacy") {
    const loaderObservations = {
        legacyCalls: 0,
        extensionCalls: 0,
        featureCalls: 0,
        logErrors: [],
    };

    class Actor {
        add() {}
        set_label_actor() {}
        disconnect() {}
    }

    class AppletBase {
        constructor(orientation, panelHeight, instanceId) {
            this.orientation = orientation;
            this.panel_height = panelHeight;
            this.instance_id = instanceId;
            this.actor = new Actor();
        }
        setAllowedLayout() {}
    }

    class Label {
        constructor() {
            this.clutter_text = {};
            this.text = "";
        }
        get_preferred_width() { return [0, this.text.length * 8]; }
        get_text() { return this.text; }
        set_text(value) { this.text = value; }
    }

    class Bin {
        constructor() {
            this.min_width = 0;
            this.child = null;
            this.relayoutQueued = 0;
        }
        _init() {
            this.min_width = 0;
            this.child = null;
            this.relayoutQueued = 0;
        }
        set_child(child) { this.child = child; }
        get_child() { return this.child; }
        queue_relayout() { this.relayoutQueued += 1; }
        vfunc_get_preferred_width() { return [0, 0]; }
    }

    class SignalBag {
        constructor() { this.entries = []; }
        connect(object, signal, callback) {
            if (!object || typeof object.connect !== "function") return 0;
            const id = object.connect(signal, callback);
            this.entries.push([object, id]);
            return id;
        }
        disconnectAll() { this.entries = []; }
    }

    const extensionModules = {
        calendar: {},
        desktopIntegration: { DesktopIntegration: class {} },
        eventManager: {},
        eventView: {},
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
    const runtimeSupport = {
        SignalBag,
        loadLocalModule(name) {
            loaderObservations.featureCalls += 1;
            return extensionModules[name];
        },
    };
    extensionModules.runtimeSupport = runtimeSupport;
    const extension = loaderMode === "extension" ? {
        getCurrentExtension() {
            loaderObservations.extensionCalls += 1;
            return { imports: extensionModules };
        },
    } : undefined;

    const context = {
        console,
        require(name) {
            loaderObservations.legacyCalls += 1;
            if (loaderMode === "extension") {
                throw new Error("legacy loader used by Cinnamon 6.7 path");
            }
            if (name === "./runtimeSupport") return runtimeSupport;
            return extensionModules[name.replace(/^\.\//, "")];
        },
        _: (value) => value,
        ngettext: (one, many, count) => count === 1 ? one : many,
        global: {
            logError(error) { loaderObservations.logErrors.push(String(error)); },
            settings: { disconnect() {}, connect() { return 1; } },
        },
        imports: {
            ui: {
                extension,
                applet: {
                    Applet: AppletBase,
                    AllowedLayout: { BOTH: 0 },
                    AppletPopupMenu: class {},
                },
                popupMenu: {},
                settings: {},
                main: {
                    keybindingManager: {},
                    themeManager: { disconnect() {}, connect() { return 1; } },
                },
            },
            gi: {
                Gio: {},
                GObject: {
                    registerClass(klass) {
                        return class extends klass {
                            constructor(...args) {
                                super(...args);
                                if (typeof this._init === "function") {
                                    this._init(...args);
                                }
                            }
                        };
                    },
                },
                Calendar: {
                    get_version() { return nativeVersion; },
                    CalendarSystem: {
                        new(id) {
                            return id === "gregorian" ? {
                                get_id() { return id; },
                            } : null;
                        },
                    },
                },
                Clutter: {},
                Pango: { EllipsizeMode: { NONE: 0 } },
                St: {
                    Label,
                    Bin,
                    Align: { MIDDLE: 0 },
                    Side: { LEFT: 0, RIGHT: 1 },
                },
                UPowerGlib: {},
                CinnamonDesktop: {
                    WallClock: {
                        lctime_format(domain, value) { return value; },
                    },
                },
            },
            misc: { util: {} },
            gettext: {
                bindtextdomain() {},
                domain() { return { gettext(value) { return value; } }; },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "applet.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__CalendarApplet = CalendarApplet;`,
        context,
        { filename: "applet.js" }
    );
    context.__CalendarApplet.loaderObservations = loaderObservations;
    return context.__CalendarApplet;
}

function evaluateDesktopIntegration() {
    let nextSignalId = 1;
    const observations = { logs: [] };

    class DesktopSettings {
        connect() { return nextSignalId++; }
        disconnect() {}
        get_boolean(key) { throw new Error(`failed ${key}`); }
        get_string(key) { throw new Error(`failed ${key}`); }
    }
    class SignalBag {
        constructor() { this.entries = []; }
        connect(object, signal, callback) {
            if (!object || typeof object.connect !== "function") return 0;
            const id = object.connect(signal, callback);
            this.entries.push([object, id]);
            return id;
        }
        disconnectAll() {
            for (const [object, id] of this.entries.splice(0)) {
                object.disconnect(id);
            }
        }
    }

    const context = {
        console,
        global: {
            logError(error) { observations.logs.push(String(error)); },
            settings: { connect() { return 1; }, disconnect() {} },
            create_app_launch_context() { return {}; },
        },
        imports: {
            gi: {
                Gio: {
                    Settings: DesktopSettings,
                    DesktopAppInfo: { new() { return null; } },
                },
                UPowerGlib: { Client: class { connect() { return 1; } disconnect() {} } },
            },
            ui: {
                main: {
                    themeManager: { connect() { return 1; }, disconnect() {} },
                    keybindingManager: {},
                },
            },
            misc: { util: { spawnCommandLine() {} } },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "desktopIntegration.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__DesktopIntegration = DesktopIntegration;`,
        context,
        { filename: "desktopIntegration.js" }
    );
    const integration = new context.__DesktopIntegration(
        { instance_id: 7 }, SignalBag, {}
    );
    return { integration, observations };
}

function evaluateEventView() {
    let eventViewIdle = null;
    const eventViewObservations = {
        spawns: [],
        availablePrograms: new Set(["gnome-calendar", "clockenstein-calendar"]),
    };
    const signals = {
        addSignalMethods(prototype) {
            prototype.connect = prototype.connect || function() { return 1; };
            prototype.emit = prototype.emit || function() {};
        },
    };
    const context = {
        console,
        _: (value) => value,
        ngettext: (one, many, count) => count === 1 ? one : many,
        require(name) {
            if (name === "./runtimeSupport") {
                return {
                    midnight() { return null; },
                    sameInstant(a, b) {
                        return a !== null && b !== null &&
                            a.to_unix() === b.to_unix();
                    },
                    SignalBag: class SignalBag {
                        connect() { return 1; }
                        disconnectAll() {}
                    },
                };
            }
            throw new Error("unexpected module " + name);
        },
        imports: {
            gi: {
                Atk: { Role: { LIST: 1, LIST_ITEM: 2 } },
                Calendar: {
                    EventDayRelation: { STARTS_ON_DAY: 1, ENDS_ON_DAY: 2 },
                    EventState: { PAST: 0, FUTURE: 1, PRESENT: 2 },
                },
                CinnamonDesktop: {
                    WallClock: {
                        lctime_format(domain, value) { return value; },
                    },
                },
                Clutter: { ActorAlign: { CENTER: 0, START: 1, END: 2 } },
                GLib: {
                    SOURCE_REMOVE: false,
                    find_program_in_path(program) {
                        return eventViewObservations.availablePrograms.has(program)
                            ? program
                            : null;
                    },
                    DateTime: {
                        new_now_local() { return null; },
                        new_local() { return null; },
                    },
                },
                Gio: { Settings: class {} },
                Pango: { EllipsizeMode: { NEVER: 0 } },
                St: {
                    PolicyType: { NEVER: 0, AUTOMATIC: 1 },
                    IconType: { SYMBOLIC: 0 },
                },
            },
            signals,
            ui: { separator: { Separator: class {} } },
            misc: {
                util: {
                    trySpawn(args) {
                        eventViewObservations.spawns.push(Array.from(args));
                    },
                },
            },
            mainloop: {
                idle_add(callback) {
                    eventViewIdle = callback;
                    return 71;
                },
                timeout_add() { return 72; },
                source_remove() { eventViewIdle = null; },
            },
            gettext: {
                bindtextdomain() {},
                domain() { return { gettext(value) { return value; } }; },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "eventView.js"),
        "utf8"
    );
    vm.runInContext(
        source +
            "\nglobalThis.__EventList = EventList;" +
            "\nglobalThis.__EventRow = EventRow;" +
            "\nglobalThis.__replaceEventRowForTest = " +
            "(replacement) => { EventRow = replacement; };",
        context,
        { filename: "eventView.js" }
    );
    return {
        EventList: context.__EventList,
        EventRow: context.__EventRow,
        observations: eventViewObservations,
        replaceEventRow: context.__replaceEventRowForTest,
        runEventViewIdle() {
            assert.notEqual(eventViewIdle, null, "an event-view idle must be pending");
            const callback = eventViewIdle;
            eventViewIdle = null;
            callback();
        },
    };
}

function evaluateCalendar() {
    let nextSignalId = 1;
    const observations = {
        actorDisconnects: 0,
        actorDestroys: 0,
        desktopDisconnects: 0,
        eventDisconnects: 0,
        removedSources: 0,
        calendarTimeoutCallback: null,
        runCalendarTimeout() {
            assert.notEqual(
                this.calendarTimeoutCallback,
                null,
                "a calendar coalescing timeout must be pending"
            );
            const callback = this.calendarTimeoutCallback;
            this.calendarTimeoutCallback = null;
            callback();
        },
    };

    class Table {
        constructor() {}
        connect() { return nextSignalId++; }
        disconnect() { observations.actorDisconnects += 1; }
        destroy() { observations.actorDestroys += 1; }
    }

    class DesktopSettings {
        connect() { return nextSignalId++; }
        disconnect() { observations.desktopDisconnects += 1; }
    }

    const signals = {
        addSignalMethods(prototype) {
            prototype.connect = prototype.connect || function() {
                return nextSignalId++;
            };
            prototype.emit = function() {};
        },
    };
    const Clutter = {
        KEY_Left: 1,
        KEY_Right: 2,
        KEY_Up: 3,
        KEY_Down: 4,
        KEY_Page_Up: 5,
        KEY_Page_Down: 6,
        KEY_Home: 7,
        KEY_End: 8,
        EVENT_PROPAGATE: 0,
        EVENT_STOP: 1,
        ModifierType: {
            SHIFT_MASK: 1 << 0,
            CONTROL_MASK: 1 << 1,
        },
    };

    const nativeBoundaryModule = {
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

    const context = {
        console,
        global: { logError() {} },
        log() {},
        require(name) {
            if (name === "./runtimeSupport") {
                return {
                    loadLocalModule(moduleName) {
                        if (moduleName === "nativeBoundary") {
                            return nativeBoundaryModule;
                        }
                        if (moduleName === "civilDate") {
                            return { CivilDateBoundary: context.__CivilDateBoundary };
                        }
                        if (moduleName === "calendarEventProjection") {
                            return {
                                CalendarEventProjection:
                                    context.__CalendarEventProjection,
                            };
                        }
                        throw new Error(`unexpected local module ${moduleName}`);
                    },
                    SignalBag: class SignalBag {
                        constructor() { this.entries = []; }
                        connect(object, signal, callback) {
                            const id = object.connect(signal, callback);
                            this.entries.push([object, id]);
                            return id;
                        }
                        disconnectAll() {
                            for (const [object, id] of this.entries.splice(0)) {
                                object.disconnect(id);
                            }
                        }
                    },
                };
            }
            throw new Error(`unexpected module ${name}`);
        },
        imports: {
            gi: {
                Clutter,
                Gio: { Settings: DesktopSettings },
                GLib: { SOURCE_REMOVE: false },
                St: { Table },
                Pango: { SCALE: 1024 },
                Cinnamon: {
                    util_get_week_start() { return 0; },
                },
                Calendar: {
                    DatePart: { DAY: 1, MONTH: 2, YEAR: 3, SHORT: 4, FULL: 5 },
                    CalendarSystem: {
                        new(id) {
                            return id === "gregorian" ? {
                                get_id() { return id; },
                            } : null;
                        },
                    },
                    date_weekday(year, month, day) {
                        const value = new Date(0);
                        value.setUTCHours(12, 0, 0, 0);
                        value.setUTCFullYear(year, month - 1, day);
                        return value.getUTCDay();
                    },
                    date_same(yearA, monthA, dayA, yearB, monthB, dayB) {
                        function valid(year, month, day) {
                            if (!Number.isInteger(year) ||
                                !Number.isInteger(month) ||
                                !Number.isInteger(day) ||
                                month < 1 || month > 12 || day < 1) {
                                return false;
                            }
                            const leap = year % 4 === 0 &&
                                (year % 100 !== 0 || year % 400 === 0);
                            const lengths = [
                                31, leap ? 29 : 28, 31, 30, 31, 30,
                                31, 31, 30, 31, 30, 31,
                            ];
                            return day <= lengths[month - 1];
                        }
                        return valid(yearA, monthA, dayA) &&
                            valid(yearB, monthB, dayB) &&
                            yearA === yearB && monthA === monthB && dayA === dayB;
                    },
                    date_is_work_day() { return true; },
                },
            },
            lang: { bind(owner, fn) { return fn.bind(owner); } },
            signals,
            ui: { main: {} },
            mainloop: {
                timeout_add(delay, callback) {
                    observations.calendarTimeoutCallback = callback;
                    return 91;
                },
                source_remove() {
                    observations.calendarTimeoutCallback = null;
                    observations.removedSources += 1;
                },
            },
            gettext: {
                bindtextdomain() {},
                domain() { return { gettext(value) { return value; } }; },
            },
        },
    };
    vm.createContext(context);

    const civilSource = fs.readFileSync(
        path.join(root, "src", "cinnamon", "civilDate.js"),
        "utf8"
    );
    vm.runInContext(
        `${civilSource}\nglobalThis.__CivilDateBoundary = CivilDateBoundary;`,
        context,
        { filename: "civilDate.js" }
    );
    const projectionSource = fs.readFileSync(
        path.join(root, "src", "cinnamon", "calendarEventProjection.js"),
        "utf8"
    );
    vm.runInContext(
        `${projectionSource}\nglobalThis.__CalendarEventProjection = CalendarEventProjection;`,
        context,
        { filename: "calendarEventProjection.js" }
    );

    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "calendar.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__Calendar = Calendar;` +
            `\nglobalThis.__localDate = _localDate;` +
            `\nglobalThis.__representableLocalDate = _representableLocalDate;`,
        context,
        { filename: "calendar.js" }
    );

    context.__Calendar.prototype._buildHeader = function() {};
    const settings = { bindWithObject() {} };
    const eventsManager = {
        connect() { return nextSignalId++; },
        disconnect() { observations.eventDisconnects += 1; },
    };

    return {
        Calendar: context.__Calendar,
        localDate: context.__localDate,
        representableLocalDate: context.__representableLocalDate,
        Clutter,
        settings,
        eventsManager,
        desktopSettings: new DesktopSettings(),
        observations,
    };
}

function evaluatePanelClock() {
    const observations = { starts: 0, stops: 0 };

    const context = {
        console,
        _: (value) => value,
        global: { logError() {} },
        imports: {
            gi: {
                Calendar: {
                    DatePart: { SHORT: 4, FULL: 5 },
                    TimeMode: { INVALID: 0 },
                    time_mode_from_string(mode) {
                        const nativeModes = [
                            "decimal", "internet", "unix", "hexadecimal",
                            "binary", "sidereal", "solar", "julian",
                            "mean-solar", "modified-julian", "chinese-time",
                            "roman-temporal", "japanese-temporal",
                            "italian-hours", "babylonian-hours",
                            "indian-ghati", "chinese-ke", "nuremberg-hours",
                            "babylonian-ancient", "nuremberg-solar",
                        ];
                        return nativeModes.includes(mode) ? mode : 0;
                    },
                    time_mode_requires_longitude(mode) {
                        return [
                            "sidereal", "solar", "mean-solar", "roman-temporal",
                            "japanese-temporal", "italian-hours", "babylonian-hours",
                        ].includes(mode);
                    },
                    time_mode_requires_latitude(mode) {
                        return [
                            "roman-temporal", "japanese-temporal",
                            "italian-hours", "babylonian-hours",
                        ].includes(mode);
                    },
                    replace_time(label, normalTime, nativeTime) {
                        if (typeof label !== "string") {
                            throw new TypeError("label must be a string");
                        }
                        const index = label.indexOf(normalTime);
                        if (index < 0) return null;
                        return label.slice(0, index) + nativeTime +
                            label.slice(index + normalTime.length);
                    },
                },
                CinnamonDesktop: {
                    WallClock: {
                        lctime_format(domain, value) { return value; },
                    },
                },
            },
        },
    };
    vm.createContext(context);
    const source = fs.readFileSync(
        path.join(root, "src", "cinnamon", "panelClock.js"),
        "utf8"
    );
    vm.runInContext(
        `${source}\nglobalThis.__PanelClock = { panelText, todayDisplay, dayName, syncNativeClock, configureWallClock, uses24HourClock, isNativeClockMode };`,
        context,
        { filename: "panelClock.js" }
    );

    const systemClock = {
        get_time() { return "@500"; },
        start() { observations.starts += 1; },
        start_at_location() { observations.starts += 1; },
        stop() { observations.stops += 1; },
    };
    return { PanelClock: context.__PanelClock, systemClock, observations };
}

function testPanelClockDefensiveFormatting() {
    const { PanelClock, systemClock, observations } = evaluatePanelClock();
    const clock = {
        get_clock() { return "12:34"; },
        get_clock_for_format() { return null; },
    };
    const base = {
        mode: "decimal",
        showSeconds: false,
        latitude: 0,
        longitude: 0,
        locationConfigured: true,
        vertical: false,
        use24h: true,
        showDate: false,
        primaryCalendar: "gregorian",
    };

    assert.equal(PanelClock.isNativeClockMode("decimal"), true);
    assert.equal(PanelClock.isNativeClockMode("standard"), false);
    assert.equal(
        PanelClock.isNativeClockMode("not-a-clock-mode"),
        true,
        "missing explicit clocks must not silently select WallClock"
    );
    assert.equal(
        PanelClock.panelText(clock, systemClock, base),
        "@500",
        "native time must remain authoritative for non-conventional modes"
    );

    const conventional = { ...base, mode: "standard" };
    assert.equal(
        PanelClock.panelText(clock, systemClock, conventional),
        "12:34",
        "conventional modes must use WallClock text"
    );

    const selectedCalendar = {
        format_date_part() { return "Positivist 13 Gutenberg 238"; },
    };
    const selectedCalendarConfig = {
        ...conventional,
        primaryCalendar: "positivist",
        use24h: false,
        showDate: true,
    };
    assert.equal(
        PanelClock.panelText(
            clock,
            systemClock,
            selectedCalendarConfig,
            selectedCalendar
        ),
        "Positivist 13 Gutenberg 238 12:34",
        "the panel date must follow the selected system calendar"
    );

    const selectedNativeConfig = {
        ...selectedCalendarConfig,
        mode: "internet",
    };
    assert.equal(
        PanelClock.panelText(
            clock,
            systemClock,
            selectedNativeConfig,
            selectedCalendar
        ),
        "Positivist 13 Gutenberg 238 @500",
        "selected calendar date must compose with native clock systems"
    );

    const unconfigured = {
        ...base,
        mode: "sidereal",
        locationConfigured: false,
    };
    assert.equal(
        PanelClock.panelText(clock, systemClock, unconfigured),
        "N/A LOC",
        "location-dependent clocks must not silently assume Greenwich"
    );
    PanelClock.syncNativeClock(systemClock, unconfigured);
    assert.equal(observations.stops, 1);

    const invalidConfiguredLocation = {
        ...base,
        mode: "sidereal",
        locationConfigured: true,
        longitude: Number.NaN,
    };
    assert.equal(
        PanelClock.panelText(clock, systemClock, invalidConfiguredLocation),
        "N/A LOC",
        "invalid configured coordinates must remain unavailable"
    );
    PanelClock.syncNativeClock(systemClock, invalidConfiguredLocation);
    assert.equal(observations.stops, 2);

    const outOfRangeConfiguredLocation = {
        ...base,
        mode: "sidereal",
        locationConfigured: true,
        longitude: 181,
    };
    assert.equal(
        PanelClock.panelText(clock, systemClock, outOfRangeConfiguredLocation),
        "N/A LOC",
        "out-of-range configured coordinates must remain unavailable"
    );
    PanelClock.syncNativeClock(systemClock, outOfRangeConfiguredLocation);
    assert.equal(observations.stops, 3);

    PanelClock.syncNativeClock(systemClock, base);
    assert.equal(observations.starts, 1);
}

function testPanelClockPreservesLocaleCasingAndNullFormats() {
    const { PanelClock, systemClock } = evaluatePanelClock();
    const clock = {
        get_clock() { return "maanantai 12:34"; },
        get_clock_for_format() { return null; },
    };
    const config = {
        mode: "standard",
        showSeconds: false,
        latitude: 0,
        longitude: 0,
        locationConfigured: false,
        vertical: false,
        use24h: true,
        showDate: true,
        primaryCalendar: "gregorian",
    };

    assert.equal(
        PanelClock.panelText(clock, systemClock, config),
        "Calendar unavailable maanantai 12:34",
        "missing date resources must be visible without rewriting clock text"
    );
    assert.equal(PanelClock.dayName(clock), "");
    const display = PanelClock.todayDisplay(clock, null, config);
    assert.equal(display.shortDate, "Calendar unavailable");
    assert.equal(display.tooltip, "Calendar unavailable");
}

function testMintFallbackClockFormatting() {
    const { PanelClock } = evaluatePanelClock();

    function render({ use24h, showDate, showSeconds, mode = "standard",
                      vertical = false, primaryCalendar = "gregorian" }) {
        let format = null;
        const clock = {
            set_format_string(value) {
                format = value;
                return true;
            },
        };
        PanelClock.configureWallClock(clock, {
            mode,
            showSeconds,
            vertical,
            primaryCalendar,
            use24h,
            showDate,
        });
        return format;
    }

    assert.equal(
        render({ use24h: true, showDate: false, showSeconds: false }),
        "%R",
        "Mint 24-hour mode must produce the conventional 24-hour clock"
    );
    assert.equal(
        render({ use24h: false, showDate: false, showSeconds: false }),
        "%-l:%M %p",
        "Mint 12-hour mode must produce the conventional AM/PM clock"
    );
    assert.equal(
        render({ use24h: true, showDate: false, showSeconds: true }),
        "%R:%S",
        "Mint seconds preference must be preserved in 24-hour mode"
    );
    assert.equal(
        render({ use24h: false, showDate: false, showSeconds: true }),
        "%-l:%M:%S %p",
        "Mint seconds preference must be preserved in 12-hour mode"
    );
    assert.equal(
        render({ use24h: true, showDate: true, showSeconds: false }),
        "%R",
        "Gregorian panel dates must come from their resource"
    );
    assert.equal(
        render({ use24h: false, showDate: true, showSeconds: true }),
        "%-l:%M:%S %p",
        "date composition must not duplicate platform Gregorian formatting"
    );
    assert.equal(
        render({
            use24h: true,
            showDate: true,
            showSeconds: false,
            primaryCalendar: "positivist",
        }),
        "%R",
        "non-Gregorian panel dates must not leak Gregorian WallClock text"
    );
    assert.equal(
        render({
            use24h: false,
            showDate: false,
            showSeconds: false,
            mode: "standard-24",
        }),
        "%R",
        "an explicit System Settings 24-hour profile must override Mint"
    );
    assert.equal(
        render({
            use24h: true,
            showDate: false,
            showSeconds: false,
            mode: "standard-12",
        }),
        "%-l:%M %p",
        "an explicit System Settings 12-hour profile must override Mint"
    );
    assert.equal(
        render({
            use24h: true,
            showDate: false,
            showSeconds: true,
            vertical: true,
        }),
        "%H%n%M%n%S",
        "vertical Mint clocks must preserve 24-hour and seconds preferences"
    );
}

function testConstructorAtomicity() {
    const MismatchedAppletClass = evaluateApplet("3.1.9");
    assert.throws(
        () => new MismatchedAppletClass(0, 24, 7, version),
        new RegExp(
            `native library 3\\.1\\.9 does not match applet ${version.replaceAll(".", "\\.")}`
        ),
        "native version mismatch must fail with the original diagnostic"
    );

    const AppletClass = evaluateApplet();
    const originalInitialise = AppletClass.prototype._initialiseState;
    const originalBuild = AppletClass.prototype._buildApplet;
    const originalDestroy = AppletClass.prototype._destroy;
    const initialiseSentinel = new Error("initialisation failed");
    const buildSentinel = new Error("construction failed");
    let destroyed = 0;

    AppletClass.prototype._destroy = function() {
        destroyed += 1;
        this._destroyed = true;
    };
    AppletClass.prototype._initialiseState = function() {
        this._destroyed = false;
        throw initialiseSentinel;
    };
    assert.throws(() => new AppletClass(0, 24, 7, version), initialiseSentinel);
    assert.equal(destroyed, 1, "failed initialisation must clean up exactly once");

    AppletClass.prototype._initialiseState = originalInitialise;
    AppletClass.prototype._buildApplet = function() { throw buildSentinel; };
    assert.throws(() => new AppletClass(0, 24, 7, version), buildSentinel);
    assert.equal(destroyed, 2, "failed construction must clean up exactly once");

    AppletClass.prototype._buildApplet = function() {};
    AppletClass.prototype._destroy = originalDestroy;
    const instance = new AppletClass(0, 24, 7, version);
    assert.equal(instance._destroyed, false);
    assert.equal(instance._calendar_system.get_id(), "gregorian");
    instance._destroy();
    instance._destroy();
    assert.equal(instance._destroyed, true, "destruction must be idempotent");

    AppletClass.prototype._buildApplet = originalBuild;
}

function testModuleLoaderCompatibility() {
    const LegacyAppletClass = evaluateApplet(version, "legacy");
    assert.equal(
        LegacyAppletClass.loaderObservations.legacyCalls,
        1,
        "Cinnamon 6.4/6.6 must bootstrap runtimeSupport through require()"
    );
    assert.equal(LegacyAppletClass.loaderObservations.extensionCalls, 0);
    assert.equal(LegacyAppletClass.loaderObservations.featureCalls, 6);

    const ExtensionAppletClass = evaluateApplet(version, "extension");
    assert.equal(
        ExtensionAppletClass.loaderObservations.extensionCalls,
        1,
        "Cinnamon 6.7 must bootstrap runtimeSupport from the extension"
    );
    assert.equal(
        ExtensionAppletClass.loaderObservations.legacyCalls,
        0,
        "the 6.7 path must not fall through to the legacy loader"
    );
    assert.equal(ExtensionAppletClass.loaderObservations.featureCalls, 6);
}

function testDesktopPreferenceFallbackDiagnostics() {
    const { integration, observations } = evaluateDesktopIntegration();

    assert.equal(integration.uses24h(), false);
    assert.equal(integration.uses24h(), false);
    assert.equal(integration.showsDate(), false);
    assert.equal(integration._themeName(), "");
    assert.equal(
        observations.logs.length,
        3,
        "each unavailable desktop preference must be diagnosed once, not per repaint"
    );
    for (const key of ["clock-use-24h", "clock-show-date", "gtk-theme"]) {
        assert.ok(
            observations.logs.some((message) =>
                message.includes(key) && message.includes("compatibility default")
            ),
            `${key} fallback must identify the failed setting`
        );
    }
    integration.destroy();
    integration.destroy();
}

function testCalendarRejectsNormalizedCivilDates() {
    const originalTimezone = process.env.TZ;
    process.env.TZ = "Pacific/Apia";
    try {
        const { localDate, representableLocalDate } = evaluateCalendar();

        assert.equal(
            localDate(2026, 2, 30),
            null,
            "invalid Gregorian coordinates must still be rejected"
        );

        const skipped = localDate(2011, 12, 30);
        assert.notEqual(
            skipped,
            null,
            "a timezone-skipped civil date must remain selectable"
        );
        assert.equal(skipped.getFullYear(), 2011);
        assert.equal(skipped.getMonth() + 1, 12);
        assert.equal(skipped.getDate(), 30);
        assert.equal(
            representableLocalDate(2011, 12, 30),
            null,
            "the regression must exercise a genuinely unrepresentable local date"
        );

        const valid = localDate(2011, 12, 31);
        assert.notEqual(valid, null);
        assert.equal(valid.getFullYear(), 2011);
        assert.equal(valid.getMonth() + 1, 12);
        assert.equal(valid.getDate(), 31);

        const extreme = localDate(2147483647, 12, 30);
        assert.notEqual(
            extreme,
            null,
            "date-only UI state must cover the native signed-gint year domain"
        );
        assert.equal(
            representableLocalDate(2147483647, 12, 30),
            null,
            "the extreme-year regression must not accidentally pass through Date"
        );
    } finally {
        if (originalTimezone === undefined) {
            delete process.env.TZ;
        } else {
            process.env.TZ = originalTimezone;
        }
    }
}

function testCalendarDayNavigationDelegatesToNative() {
    const { Calendar } = evaluateCalendar();
    const calendar = Object.create(Calendar.prototype);
    let call = null;
    calendar._calendarSystem = {
        add_days_parts(year, month, day, amount) {
            call = [year, month, day, amount];
            if (amount === 2) {
                return null;
            }
            return {
                deep_unpack() { return [year, month, day + amount]; },
            };
        },
    };
    const source = new Date(2026, 7, 8, 12, 0, 0);
    const next = calendar._dateByDays(source, 1);
    assert.deepEqual(call, [2026, 8, 8, 1]);
    assert.equal(next.getFullYear(), 2026);
    assert.equal(next.getMonth() + 1, 8);
    assert.equal(next.getDate(), 9);
    assert.equal(calendar._dateByDays(source, 2), null);
}

function testCalendarRejectsUnsupportedProviderDate() {
    const { Calendar } = evaluateCalendar();
    const original = new Date(2026, 6, 29, 12, 0, 0);
    const unsupported = new Date(1800, 0, 1, 12, 0, 0);
    const calendar = Object.create(Calendar.prototype);
    let updates = 0;
    let emissions = 0;

    Object.assign(calendar, {
        _destroyed: false,
        _selectedDate: original,
        _gridModelForDate() { return null; },
        _update() { updates += 1; },
        emit() { emissions += 1; },
    });

    calendar.setDate(unsupported, false);
    assert.equal(calendar._selectedDate, original);
    assert.equal(updates, 0);
    assert.equal(emissions, 0);
}

function testCalendarSelectedDateIsolation() {
    const { Calendar } = evaluateCalendar();
    const calendar = Object.create(Calendar.prototype);
    let emitted = null;

    Object.assign(calendar, {
        _destroyed: false,
        _selectedDate: new Date(2026, 6, 29, 12, 0, 0),
        _gridModelForDate() { return {}; },
        _update() {},
        emit(signal, date) {
            if (signal === "selected-date-changed") emitted = date;
        },
    });

    const external = calendar.getSelectedDate();
    external.setFullYear(2030);
    assert.equal(
        calendar._selectedDate.getFullYear(),
        2026,
        "getter callers must not be able to mutate Calendar-owned selection state"
    );

    calendar.setDate(new Date(2026, 6, 30, 12, 0, 0), false);
    assert.notEqual(emitted, null);
    emitted.setFullYear(2030);
    assert.equal(
        calendar._selectedDate.getFullYear(),
        2026,
        "selection signal receivers must receive a detached civil-date value"
    );
    assert.equal(calendar._selectedDate.getDate(), 30);
}

function testCalendarGridFailureIsTransactional() {
    const { Calendar } = evaluateCalendar();
    const calendar = Object.create(Calendar.prototype);
    let removed = 0;

    Object.assign(calendar, {
        _destroyed: false,
        _calendarSystem: {},
        _selectedDate: new Date(2026, 6, 29, 12, 0, 0),
        _monthLabel: { text: "old month" },
        _yearLabel: { text: "old year" },
        _gridModelForDate() { return null; },
        _removeCells() { removed += 1; },
    });

    calendar._update(false);
    assert.equal(removed, 0);
    assert.equal(calendar._monthLabel.text, "old month");
    assert.equal(calendar._yearLabel.text, "old year");
}

function testCalendarLifecycle() {
    const { Calendar, eventsManager, observations } = evaluateCalendar();
    const calendar = new Calendar(eventsManager, 0, false);

    calendar._update_id = 41;
    calendar._set_date_idle_id = 42;
    calendar.destroy();
    calendar.destroy();

    assert.equal(
        observations.removedSources,
        2,
        "calendar must release both pending Mainloop sources"
    );
    assert.equal(
        observations.desktopDisconnects,
        0,
        "calendar must not own desktop-settings signals below the composition root"
    );
    assert.equal(
        observations.eventDisconnects,
        3,
        "calendar must disconnect every EventsManager signal"
    );
    assert.equal(
        observations.actorDisconnects,
        2,
        "calendar must disconnect its long-lived actor signals"
    );
    assert.equal(
        observations.actorDestroys,
        1,
        "calendar actor destruction must be idempotent"
    );
    assert.equal(calendar._destroyed, true);
}

function testCalendarUnavailableNavigationRecovers() {
    const { Calendar, eventsManager } = evaluateCalendar();
    const calendar = new Calendar(eventsManager, 0, false);
    const destinations = [];
    calendar._monthLabel = { text: "" };
    calendar._yearLabel = { text: "" };
    calendar._removeCells = () => {};
    calendar._gridModelForDate = () => ({});
    calendar._update = () => {};
    calendar.queue_set_date = date => destinations.push(date);

    calendar.setCalendarSystem("missing-calendar");
    assert.equal(calendar._monthLabel.text, "Calendar unavailable");
    for (const [years, months] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) {
        assert.doesNotThrow(() => calendar._browse(years, months));
    }
    assert.equal(destinations.length, 0);

    calendar.setCalendarSystem("gregorian");
    calendar._calendarSystem.add_months_parts = () => ({
        deep_unpack() { return [2026, 11, 23]; },
    });
    calendar._browse(0, 1);
    assert.equal(destinations.length, 1);
    assert.equal(destinations[0].getFullYear(), 2026);
    assert.equal(destinations[0].getMonth(), 10);
    assert.equal(destinations[0].getDate(), 23);
    calendar.destroy();
}

function testCalendarNavigationCoalescing() {
    const { Calendar, eventsManager, observations } = evaluateCalendar();
    const calendar = new Calendar(eventsManager, 0, false);
    const applied = [];
    calendar.setDate = (date) => applied.push(date.getTime());

    const first = new Date(2026, 8, 21, 12, 0, 0);
    const second = new Date(2026, 8, 22, 12, 0, 0);
    calendar.queue_set_date(first);
    calendar.queue_set_date(second);
    observations.runCalendarTimeout();
    assert.deepEqual(
        applied,
        [second.getTime()],
        "the newest accumulated destination must win without dropping it"
    );
    calendar.destroy();
}

function testCalendarKeyboardNavigation() {
    const { Calendar, Clutter } = evaluateCalendar();
    const calendar = Object.create(Calendar.prototype);
    const calls = [];
    const date = new Date(2026, 8, 20, 12, 0, 0);

    calendar._focusAfterUpdate = null;
    calendar._weekStart = 0;
    calendar._browse = (yearDelta, periodDelta, baseDate, focus) => {
        calls.push([yearDelta, periodDelta, baseDate, focus]);
    };
    calendar._queueKeyboardDate = () => {
        throw new Error("Page navigation must use the native period browser.");
    };

    function keyEvent(key, state = 0) {
        return {
            get_key_symbol() { return key; },
            get_state() { return state; },
        };
    }

    assert.equal(
        calendar._onDayKeyPress(date, keyEvent(Clutter.KEY_Page_Up)),
        Clutter.EVENT_STOP
    );
    assert.deepEqual(
        calls.pop().slice(0, 2),
        [0, -1],
        "Page Up must move by one calendar period"
    );

    assert.equal(
        calendar._onDayKeyPress(
            date,
            keyEvent(Clutter.KEY_Page_Down, Clutter.ModifierType.SHIFT_MASK)
        ),
        Clutter.EVENT_STOP
    );
    assert.deepEqual(
        calls.pop().slice(0, 2),
        [1, 0],
        "Shift+Page Down must move by one calendar year"
    );

    calendar._onDayKeyPress(
        date,
        keyEvent(Clutter.KEY_Page_Up, Clutter.ModifierType.CONTROL_MASK)
    );
    assert.deepEqual(
        calls.pop().slice(0, 2),
        [0, -1],
        "Control must not silently replace the standard Shift year modifier"
    );
}

function testEventListLaunchesSelectedCinnamonBackend() {
    const { EventList, observations } = evaluateEventView();
    const view = Object.create(EventList.prototype);
    Object.assign(view, {
        _destroyed: false,
        _canLaunchCalendar: true,
        _backend: "clockenstein",
        selected_date: {
            format(pattern) {
                return pattern === "%F" ? "2026-09-29" : "29/09/26";
            },
        },
    });

    view.launch_calendar(view.selected_date);
    assert.deepEqual(
        observations.spawns.pop(),
        ["clockenstein-calendar", "--date=2026-09-29"],
        "Clockenstein backend must launch its matching calendar application"
    );

    view._backend = "eds";
    view._launchEvent("source:event");
    assert.deepEqual(
        observations.spawns.pop(),
        ["gnome-calendar", "--uuid", "source:event"],
        "EDS backend event activation must retain GNOME Calendar UUID routing"
    );

    view._backend = "clockenstein";
    view._launchEvent("source:event");
    assert.deepEqual(
        observations.spawns.pop(),
        ["clockenstein-calendar", "--date=2026-09-29"],
        "Clockenstein has no UUID launch contract; open the selected date instead"
    );
}

function testEventListBackendRefreshesExistingRows() {
    const { EventList, observations } = evaluateEventView();
    let backend = "eds";
    const rowStates = [];
    const selectedButton = { reactive: true, can_focus: true };
    const emptyButton = { reactive: true, can_focus: true };
    const view = Object.create(EventList.prototype);

    Object.assign(view, {
        _destroyed: false,
        _backendSettings: {
            get_string(key) {
                assert.equal(key, "calendar-backend");
                return backend;
            },
        },
        _rows: [{ set_clickable(value) { rowStates.push(Boolean(value)); } }],
        selected_date_label: selectedButton,
        no_events_button: emptyButton,
    });

    observations.availablePrograms.delete("clockenstein-calendar");
    backend = "clockenstein";
    view._refreshCalendarLauncher();
    assert.equal(view._canLaunchCalendar, false);
    assert.equal(selectedButton.reactive, false);
    assert.equal(emptyButton.can_focus, false);
    assert.deepEqual(rowStates, [false]);

    observations.availablePrograms.add("clockenstein-calendar");
    view._refreshCalendarLauncher();
    assert.equal(view._canLaunchCalendar, true);
    assert.equal(selectedButton.can_focus, true);
    assert.equal(emptyButton.reactive, true);
    assert.deepEqual(rowStates, [false, true]);
}

function testTimedMultiDayEventRemainsCurrent() {
    const { EventRow } = evaluateEventView();
    const pseudoClasses = [];
    let countdown = "";
    const row = Object.create(EventRow.prototype);

    Object.assign(row, {
        event: {
            summary: "Long running event",
            all_day: false,
            multi_day: true,
            timing() { return [2, -3600, 3600]; },
            relation_to_day() { return 0; },
        },
        selected_date: {},
        event_time: {
            set_style_class_name() {},
            add_style_pseudo_class(value) { pseudoClasses.push(value); },
            set_text() {},
        },
        countdown_label: {
            set_text(value) { countdown = value; },
            get_text() { return countdown; },
            add_style_pseudo_class(value) { pseudoClasses.push(value); },
        },
        actor: { set_accessible_name() {} },
        _resetStateStyles() {
            countdown = "";
            this.is_current_or_next = false;
        },
        _rangeText() { return "range"; },
        is_current_or_next: false,
    });

    row.update_variations({});
    assert.equal(row.is_current_or_next, true);
    assert.equal(countdown, "In progress");
    assert.equal(pseudoClasses.includes("all-day"), false);
}

function testEventListScrollUsesViewport() {
    const { EventList, runEventViewIdle } = evaluateEventView();
    const view = Object.create(EventList.prototype);
    let scrollValue = null;
    const adjustment = {
        lower: 0,
        upper: 1000,
        page_size: 200,
        set_value(value) { scrollValue = value; },
    };

    Object.assign(view, {
        _destroyed: false,
        _scroll_to_idle_id: 0,
        events_scroll_box: {
            get_vscroll_bar() {
                return { get_adjustment() { return adjustment; } };
            },
        },
    });

    view._queueScrollTo({ actor: { y: 700, height: 40 } });
    runEventViewIdle();
    assert.equal(
        scrollValue,
        620,
        "agenda scroll centring must use the 200px visible page, not content height"
    );

    scrollValue = null;
    view._queueScrollTo({ actor: { y: 980, height: 40 } });
    runEventViewIdle();
    assert.equal(
        scrollValue,
        800,
        "agenda scroll centring must clamp to upper minus page size"
    );
}

function testEventListUnavailableState() {
    const { EventList } = evaluateEventView();
    const view = Object.create(EventList.prototype);
    let label = "";
    let shown = 0;
    Object.assign(view, {
        no_events_label: { set_text(value) { label = value; } },
        no_events_box: { show() { shown += 1; } },
        _no_events_timeout_id: 0,
    });

    view._showEmptyState(false, true);
    assert.equal(label, "Calendar unavailable");
    assert.equal(shown, 1);
    view._showEmptyState(false, false);
    assert.equal(label, "No Events");
}

function testEventListCacheIdentity() {
    const { EventList, replaceEventRow } = evaluateEventView();
    class FakeRow {
        constructor(event, selectedDate, params) {
            this.event = event;
            this.selected_date = selectedDate;
            this.use_24h = params.use_24h;
            this.actor = {};
            this.is_current_or_next = false;
            this.variationUpdates = 0;
        }
        connect() {}
        update_variations() { this.variationUpdates += 1; }
    }
    replaceEventRow(FakeRow);

    const view = Object.create(EventList.prototype);
    const actors = [];
    Object.assign(view, {
        _destroyed: false,
        selected_date: null,
        _rows: [],
        _current_event_cache_key: null,
        _canLaunchCalendar: false,
        _use24h: true,
        selected_date_label: {
            set_label() {},
            set_accessible_name() {},
        },
        no_events_box: { hide() {}, show() {} },
        events_box: {
            add_actor(actor) { actors.push(actor); },
            get_children() { return []; },
        },
        _cancelScroll() {},
        _cancelEmptyDelay() {},
        _showEmptyState() {},
        _queueScrollTo() {},
        _clearRows() {
            actors.length = 0;
            this._rows = [];
        },
    });

    function date(unix, label) {
        return {
            to_unix() { return unix; },
            format() { return label; },
        };
    }
    function snapshot(summary) {
        return {
            revision: 55,
            get_event_list() {
                return [{ summary, color: "#000000" }];
            },
        };
    }

    const monday = date(1789344000, "Monday");
    const tuesday = date(1789430400, "Tuesday");
    view.set_date(monday);
    view.set_events(snapshot("Monday event"), false);
    assert.equal(view._rows[0].event.summary, "Monday event");

    view.set_date(tuesday);
    view.set_events(snapshot("Tuesday event"), false);
    assert.equal(
        view._rows[0].event.summary,
        "Tuesday event",
        "same store revision on a new selected day must rebuild the agenda"
    );

    const tuesdayRow = view._rows[0];
    view.set_use_24h(false);
    view.set_events(snapshot("Tuesday event"), false);
    assert.notEqual(
        view._rows[0],
        tuesdayRow,
        "12/24-hour preference is part of the rendered agenda identity"
    );
    assert.equal(view._rows[0].use_24h, false);

    const stableRow = view._rows[0];
    view.set_events(snapshot("Tuesday event"), false);
    assert.equal(view._rows[0], stableRow);
    assert.equal(stableRow.variationUpdates, 1);
}

testPanelClockDefensiveFormatting();
testPanelClockPreservesLocaleCasingAndNullFormats();
testMintFallbackClockFormatting();
testConstructorAtomicity();
testModuleLoaderCompatibility();
testDesktopPreferenceFallbackDiagnostics();
testCalendarRejectsNormalizedCivilDates();
testCalendarDayNavigationDelegatesToNative();
testCalendarRejectsUnsupportedProviderDate();
testCalendarSelectedDateIsolation();
testCalendarGridFailureIsTransactional();
testCalendarLifecycle();
testCalendarUnavailableNavigationRecovers();
testCalendarNavigationCoalescing();
testCalendarKeyboardNavigation();
testEventListLaunchesSelectedCinnamonBackend();
testEventListBackendRefreshesExistingRows();
testTimedMultiDayEventRemainsCurrent();
testEventListScrollUsesViewport();
testEventListUnavailableState();
testEventListCacheIdentity();
/* EventsManager and CalendarServer transport are exercised through injected
 * behavioural ports in test-event-manager.js and test-calendar-server-transport.js.
 * Keeping those tests separate prevents private transport layout from becoming
 * part of the presentation runtime contract again. */
console.log("JavaScript runtime lifecycle tests passed.");
