// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar's Cinnamon presentation composition boundary.
 *
 * The applet controller loads one shell module and receives narrow classes for
 * popup compatibility, panel presentation, popup composition and the month
 * view's event-source port. Individual implementations stay split into small
 * modules while the controller remains a composition root.
 */

function _loadRuntimeSupport() {
    try {
        const Extension = imports.ui.extension;
        if (Extension && typeof Extension.getCurrentExtension === "function") {
            const extension = Extension.getCurrentExtension();
            if (extension && extension.imports && extension.imports.runtimeSupport) {
                return extension.imports.runtimeSupport;
            }
        }
    } catch (error) {
        /* Fall through when Cinnamon's current-extension lookup is unavailable. */
    }
    return require("./runtimeSupport");
}

const RuntimeSupport = _loadRuntimeSupport();
const CalendarEventSourceModule = RuntimeSupport.loadLocalModule("calendarEventSource");
const EventViewModule = RuntimeSupport.loadLocalModule("eventView");
const PanelViewModule = RuntimeSupport.loadLocalModule("panelView");
const PopupMenuModule = RuntimeSupport.loadLocalModule("popupMenu");
const PopupViewModule = RuntimeSupport.loadLocalModule("popupView");

var CalendarEventSource = CalendarEventSourceModule.CalendarEventSource;
var EventList = EventViewModule.EventList;
var PanelClockView = PanelViewModule.PanelClockView;
var CalendarPopupMenu = PopupMenuModule.CalendarPopupMenu;
var PopupView = PopupViewModule.PopupView;
