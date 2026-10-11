#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const root = path.resolve(__dirname, "..");
let nextSignalId = 1;
const timers = new Map();

class Actor {
    constructor() {
        this.reactive = true;
        this.visible = true;
        this.opacity = 255;
        this.size = null;
    }
    is_finalized() { return false; }
    hide() { this.visible = false; }
    set_size(width, height) { this.size = [width, height]; }
    remove_all_transitions() {}
    get_preferred_size() { return [0, 0, 320, 500]; }
}

class BasePopupMenu {
    constructor() {
        this.actor = new Actor();
        this.isOpen = false;
        this.passEvents = false;
        this._signals = new Map();
    }
    connect(signal, callback) {
        const id = nextSignalId++;
        this._signals.set(id, [signal, callback]);
        return id;
    }
    _emit(signal, ...args) {
        for (const [name, callback] of this._signals.values()) {
            if (name === signal) callback(this, ...args);
        }
    }
    open() {
        this.isOpen = true;
        this.actor.visible = true;
        this._emit("open-state-changed", true);
    }
    close() {
        this.isOpen = false;
        this.actor.visible = false;
        this._emit("open-state-changed", false);
        this._emit("menu-animated-closed");
    }
    setOrientation() {}
    _calculatePosition() { return [10, 20]; }
    destroy() {}
}

const context = {
    console,
    global: {
        logError() {},
        workspace_manager: {
            get_active_workspace() {
                return {
                    get_work_area_for_monitor() {
                        return { x: 100, width: 1400 };
                    },
                };
            },
        },
    },
    imports: {
        ui: {
            applet: { AppletPopupMenu: BasePopupMenu },
            main: {
                layoutManager: {
                    findMonitorForActor() { return { index: 0, x: 0, width: 1920 }; },
                },
            },
        },
        gi: {
            GLib: {
                PRIORITY_DEFAULT: 0,
                SOURCE_REMOVE: false,
                timeout_add(priority, delay, callback) {
                    const id = nextSignalId++;
                    timers.set(id, callback);
                    return id;
                },
                source_remove(id) { timers.delete(id); },
            },
            St: { Side: { TOP: 0, BOTTOM: 1, LEFT: 2, RIGHT: 3 } },
        },
    },
};
vm.createContext(context);
const source = fs.readFileSync(
    path.join(root, "src", "cinnamon", "popupMenu.js"), "utf8"
);
vm.runInContext(
    `${source}\nglobalThis.__CalendarPopupMenu = CalendarPopupMenu;`,
    context,
    { filename: "popupMenu.js" }
);
const CalendarPopupMenu = context.__CalendarPopupMenu;
const launcher = { locationLabel: "right", actor: {} };
const menu = new CalendarPopupMenu(launcher, context.imports.gi.St.Side.TOP);

assert.equal(menu.actor.reactive, false, "closed popup must not intercept input");
menu.open(false);
assert.equal(menu.actor.reactive, true, "opening must restore popup input");
menu.setEventPassthrough(true);
assert.equal(menu.passEvents, true, "event passthrough is allowed only while open");
menu.close(false);
assert.equal(menu.passEvents, false, "closing must revoke event passthrough");
assert.equal(menu.actor.reactive, false, "closing must revoke popup input");
assert.equal(menu.actor.visible, false, "completed close must hide the actor");
menu.setEventPassthrough(true);
assert.equal(menu.passEvents, false, "a closed popup must stay non-interactive");
assert.deepEqual(
    Array.from(menu._calculatePosition()),
    [1180, 20],
    "right-edge placement must use the active workspace work area"
);
menu.destroy();
console.log("Popup lifecycle behaviour tests passed.");
