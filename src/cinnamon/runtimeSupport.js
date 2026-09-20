// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Small Cinnamon/GJS compatibility primitives shared by Calendar views.
 *
 * This module owns only runtime mechanics that are independent of any calendar
 * feature: local-module resolution across Cinnamon 6.4/6.6/6.7 and deterministic
 * signal ownership.  Keeping these primitives here prevents each view from
 * growing its own slightly different teardown implementation.
 */

function loadLocalModule(name) {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports[name]) {
                return extension.imports[name];
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }

    return require(`./${name}`);
}

var midnight = function midnight(dateTime) {
    const GLib = imports.gi.GLib;
    return GLib.DateTime.new_local(
        dateTime.get_year(),
        dateTime.get_month(),
        dateTime.get_day_of_month(),
        0, 0, 0
    );
};

var sameInstant = function sameInstant(a, b) {
    return a !== null && b !== null && a.to_unix() === b.to_unix();
};

var SignalBag = class SignalBag {
    constructor() {
        this._connections = [];
    }

    connect(object, signal, callback) {
        if (!object || typeof object.connect !== "function") {
            return 0;
        }

        const id = object.connect(signal, callback);
        this._connections.push([object, id]);
        return id;
    }

    disconnectAll() {
        for (const [object, id] of this._connections.splice(0)) {
            if (!object || id <= 0 || typeof object.disconnect !== "function") {
                continue;
            }
            try {
                object.disconnect(id);
            } catch (error) {
                global.logError(error);
            }
        }
    }
};
