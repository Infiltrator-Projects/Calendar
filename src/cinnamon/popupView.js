// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/* Popup actor composition kept outside the applet controller. */

const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const PopupMenu = imports.ui.popupMenu;
const Gettext = imports.gettext;

const UUID = "calendar-plus@the-infiltratr";
Gettext.bindtextdomain(UUID, "/usr/share/locale");
const CalendarPlusGettext = Gettext.domain(UUID);

function CP_(text) {
    return CalendarPlusGettext.gettext(text);
}

var PopupView = class PopupView {
    constructor(menu, eventList, calendar, callbacks) {
        if (!menu || !eventList || !calendar) {
            throw new Error("Calendar popup requires menu, agenda and calendar views");
        }
        this._menu = menu;
        this._eventList = eventList;
        this._calendar = calendar;
        this._callbacks = callbacks || {};

        this._body = new St.BoxLayout({
            style_class: "calendar-main-box",
            vertical: false,
        });
        this._menu.addActor(this._body);

        this._calendarColumn = new St.BoxLayout({ vertical: true });
        this._todayButton = new St.Button({
            style_class: "calendar-today-home-button",
            x_align: Clutter.ActorAlign.CENTER,
            reactive: true,
            can_focus: true,
            accessible_name: CP_("Show today"),
        });
        this._todayBox = new St.BoxLayout({ vertical: true });
        this._todayButton.set_child(this._todayBox);
        this._todayButton.connect("clicked", () => {
            if (typeof this._callbacks.onResetCalendar === "function") {
                this._callbacks.onResetCalendar();
            }
        });

        this._dayLabel = new St.Label({
            style_class: "calendar-today-day-label",
        });
        this._dateLabel = new St.Label({
            style_class: "calendar-today-date-label",
        });
        this._todayBox.add_actor(this._dayLabel);
        this._todayBox.add_actor(this._dateLabel);
        this._calendarColumn.add_actor(this._todayButton);
        this._calendarColumn.add_actor(this._calendar.actor);

        /* Established composition: agenda left, month view right. */
        this._body.add_actor(this._eventList.actor);
        this._body.add_actor(this._calendarColumn);

        this._menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        const dateTimeSettings = new PopupMenu.PopupMenuItem(
            _("Date and Time Settings")
        );
        dateTimeSettings.connect("activate", () => {
            if (typeof this._callbacks.onLaunchSettings === "function") {
                this._callbacks.onLaunchSettings();
            }
        });
        this._menu.addMenuItem(dateTimeSettings);

        const aboutItem = new PopupMenu.PopupMenuItem(CP_("About Calendar"));
        aboutItem.connect("activate", () => {
            if (typeof this._callbacks.onAbout === "function") {
                this._callbacks.onAbout();
            }
        });
        this._menu.addMenuItem(aboutItem);
    }

    updateToday(dayName, shortDate, selectedToday) {
        if (!this._todayButton) {
            return;
        }
        this._todayButton.reactive = !selectedToday;
        this._todayButton.set_style_class_name(
            selectedToday
                ? "calendar-today-home-button"
                : "calendar-today-home-button-enabled"
        );
        this._dayLabel.set_text(dayName);
        this._dateLabel.set_text(shortDate);
        this._todayButton.set_accessible_name(
            `${CP_("Show today")}: ${dayName}, ${shortDate}`
        );
    }

    setAgendaVisible(visible) {
        if (this._eventList && this._eventList.actor) {
            this._eventList.actor.visible = Boolean(visible);
        }
        this.rebalanceWidth();
    }

    rebalanceWidth() {
        if (!this._calendarColumn || !this._eventList ||
            !this._eventList.actor) {
            return;
        }
        this._calendarColumn.min_width = 0;
        if (!this._eventList.actor.visible) {
            return;
        }

        const [, agendaNatural] =
            this._eventList.actor.get_preferred_width(-1);
        const [, calendarNatural] =
            this._calendarColumn.get_preferred_width(-1);
        if (agendaNatural <= 0 || calendarNatural <= 0 ||
            agendaNatural <= calendarNatural) {
            return;
        }

        this._calendarColumn.min_width = Math.ceil(Math.min(
            agendaNatural,
            calendarNatural * 1.35
        ));
    }

    destroy() {
        this._callbacks = null;
        this._calendar = null;
        this._eventList = null;
        this._menu = null;
        this._body = null;
        this._calendarColumn = null;
        this._todayButton = null;
        this._todayBox = null;
        this._dayLabel = null;
        this._dateLabel = null;
    }
};
