// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/* Panel-label presentation isolated from the applet controller. */

const GObject = imports.gi.GObject;
const Pango = imports.gi.Pango;
const St = imports.gi.St;

const LatchedWidthBin = GObject.registerClass(
class LatchedWidthBin extends St.Bin {
    _init(params = {}) {
        super._init(params);
        this._latchedWidth = 0;
    }

    resetLatch() {
        this._latchedWidth = 0;
        this.updateLatch();
    }

    updateLatch() {
        const label = this.get_child();
        if (!label) {
            return;
        }
        const [, naturalWidth] = label.get_preferred_width(-1);
        if (naturalWidth <= 0) {
            return;
        }
        if (naturalWidth > this._latchedWidth) {
            this._latchedWidth = naturalWidth;
            this.queue_relayout();
        }
    }

    vfunc_get_preferred_width(forHeight) {
        const [minimum, natural] = super.vfunc_get_preferred_width(forHeight);
        return [
            Math.max(minimum, this._latchedWidth),
            Math.max(natural, this._latchedWidth),
        ];
    }
});

var PanelClockView = class PanelClockView {
    constructor(appletActor) {
        if (!appletActor) {
            throw new Error("Calendar panel actor is required");
        }
        if (typeof appletActor.add_style_class_name === "function") {
            appletActor.add_style_class_name("calendar-plus-applet");
        }

        this.label = new St.Label({
            style_class: "applet-label calendar-plus-panel-clock",
        });
        this.label.reactive = true;
        this.label.track_hover = true;
        this.label.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        this._holder = new LatchedWidthBin({ x_align: St.Align.END });
        this._holder.set_child(this.label);
        appletActor.add(this._holder, {
            y_align: St.Align.MIDDLE,
            y_fill: false,
        });
        appletActor.set_label_actor(this.label);
    }

    setText(text) {
        if (!this.label || !text) {
            return;
        }
        this.label.set_text(text);
        this.updateWidth();
    }

    resetWidth() {
        if (this._holder) {
            this._holder.resetLatch();
        }
    }

    updateWidth() {
        if (this._holder) {
            this._holder.updateLatch();
        }
    }

    destroy() {
        this.label = null;
        this._holder = null;
    }
};
