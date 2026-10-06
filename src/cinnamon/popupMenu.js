// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Cinnamon popup compatibility boundary.
 *
 * All version-sensitive AppletPopupMenu lifecycle and placement behaviour is
 * deliberately contained here. The applet controller only uses the small
 * public surface exported by CalendarPopupMenu and never reaches into
 * Cinnamon popup internals directly.
 */

const Applet = imports.ui.applet;
const GLib = imports.gi.GLib;
const Main = imports.ui.main;
const St = imports.gi.St;

const POPUP_CLOSE_GUARD_MS = 750;

var CalendarPopupMenu = class CalendarPopupMenu extends Applet.AppletPopupMenu {
    constructor(launcher, orientation) {
        super(launcher, orientation);
        this._calendarLauncher = launcher;
        this._calendarOrientation = orientation;
        this._closeGuardSource = 0;
        this._setActorReactive(false);

        this.connect("menu-animated-closed", () => {
            if (!this.isOpen) {
                this._cancelCloseGuard();
                this._finishClosedState();
            }
        });
        this.connect("open-state-changed", (menu, open) => {
            if (open) {
                this._setActorReactive(true);
            } else {
                this._enforceClosedInputState();
            }
        });
    }

    _setActorReactive(reactive) {
        if (this.actor && !this.actor.is_finalized()) {
            this.actor.reactive = Boolean(reactive);
        }
    }

    setEventPassthrough(enabled) {
        this.passEvents = this.isOpen && Boolean(enabled);
    }

    setOrientation(orientation) {
        this._calendarOrientation = orientation;
        super.setOrientation(orientation);
    }

    _cancelCloseGuard() {
        if (this._closeGuardSource === 0) {
            return;
        }
        GLib.source_remove(this._closeGuardSource);
        this._closeGuardSource = 0;
    }

    _enforceClosedInputState() {
        this.passEvents = false;
        this._setActorReactive(false);
    }

    _finishClosedState() {
        this._enforceClosedInputState();
        if (this.isOpen || !this.actor || this.actor.is_finalized()) {
            return;
        }
        if (typeof this.actor.remove_all_transitions === "function") {
            this.actor.remove_all_transitions();
        }
        /*
         * Cinnamon exposes animating as part of AppletPopupMenu's practical
         * compatibility surface. Keep that one recovery touch confined here;
         * no feature/controller code depends on it.
         */
        this.animating = false;
        this.actor.hide();
        this.actor.set_size(-1, -1);
        this.actor.opacity = 255;
    }

    open(animate) {
        this._cancelCloseGuard();
        this.passEvents = false;
        this._setActorReactive(true);
        super.open(animate);
    }

    close(animate) {
        if (!this.isOpen) {
            this._cancelCloseGuard();
            this._finishClosedState();
            return;
        }

        super.close(animate);
        this._enforceClosedInputState();

        if (!this.animating) {
            this._finishClosedState();
            return;
        }

        this._cancelCloseGuard();
        this._closeGuardSource = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            POPUP_CLOSE_GUARD_MS,
            () => {
                this._closeGuardSource = 0;
                this._finishClosedState();
                return GLib.SOURCE_REMOVE;
            }
        );
    }

    destroy() {
        this._cancelCloseGuard();
        this._finishClosedState();
        this._calendarLauncher = null;
        super.destroy();
    }

    _calculatePosition() {
        const [xPos, yPos] = super._calculatePosition();
        const launcher = this._calendarLauncher;

        if ((this._calendarOrientation !== St.Side.TOP &&
             this._calendarOrientation !== St.Side.BOTTOM) ||
            !launcher || launcher.locationLabel !== "right" ||
            !launcher.actor) {
            return [xPos, yPos];
        }

        const monitor = Main.layoutManager.findMonitorForActor(launcher.actor);
        if (!monitor) {
            return [xPos, yPos];
        }

        let workArea = null;
        try {
            const workspace = global.workspace_manager.get_active_workspace();
            if (workspace) {
                workArea = workspace.get_work_area_for_monitor(monitor.index);
            }
        } catch (error) {
            workArea = null;
        }
        if (!workArea) {
            workArea = monitor;
        }

        const [, , naturalWidth] = this.actor.get_preferred_size();
        if (!Number.isFinite(naturalWidth) || naturalWidth <= 0) {
            return [xPos, yPos];
        }

        const rightEdge = workArea.x + workArea.width;
        return [Math.max(workArea.x, rightEdge - naturalWidth), yPos];
    }
};
