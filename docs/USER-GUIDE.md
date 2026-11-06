# Calendar

Native Cinnamon panel clock and calendar | User manual | Version 1.0 series

This manual explains the Calendar application: its panel control, popup, navigation, events, appearance, keyboard shortcut and interaction with system settings. Calendar does **not** install the separate calendar or clock resource packages. Each resource has its own documentation.

<!-- PAGE -->
## 1. Getting started

**What Calendar is.** Calendar is a Cinnamon panel applet. The panel label shows a clock; selecting it opens a popup with a calendar and, when available and enabled, an events agenda. Calendar works alongside Cinnamon's built-in calendar applet rather than silently replacing or disabling it.

**Add it to the panel**

1. Open Cinnamon **System Settings > Applets** (or the panel's **Applets** management interface).
2. Locate **Calendar**, identifiable by the Infiltrator Calendar icon, and add it to the desired panel.
3. Select the Calendar clock on the panel. The popup opens; selecting the clock again toggles it closed.
4. Open the applet's configuration from Cinnamon's Applets interface to adjust Calendar-specific options.

**First five things to try**

- Select a date in the grid; the agenda follows the selected date when the event service is available.
- Use the month arrows to browse neighbouring periods, and the year arrows to jump years.
- Select the day/date heading above the grid to return to today.
- Change **Theme** between Follow system, Day and Night in applet settings.
- Try **Super+C** to open the calendar, unless you have changed the shortcut or another component uses it.

**Compatibility.** The application's metadata declares Cinnamon 6.4, 6.6 and 6.7. Language, local time zone, weekday labels and desktop date preferences come from Cinnamon and the operating system unless a valid shared temporal policy provides an explicit alternative.

<!-- PAGE -->
## 2. Understanding the popup

<!-- DIAGRAM:popup -->

The labelled diagram is a **schematic**, not a screenshot. The left-hand area is the events agenda; the right-hand area contains the selected day's heading and the calendar grid. The agenda may be hidden when no usable event calendars are available or when events are switched off.

**A. Panel clock.** Select the clock to open or close Calendar. Its displayed time follows the active desktop preferences or a valid shared clock policy.

**B. Agenda.** Shows events for the selected date, with event times, titles and the available status information. Selecting an event can open it in the configured desktop calendar application.

**C. Today heading.** Shows today's day and date above the grid; select it after browsing to return to the current day.

**D. Month and year navigation.** Separate previous/next controls change the visible month/period or year. The ordering of month and year headings follows the current locale.

**E. Date grid.** Select any visible date. Event indicators appear where the desktop event service supplies matching events.

**F. Footer.** **Date and Time Settings** opens the appropriate settings interface; **About Calendar** opens the application's About dialog.

<!-- PAGE -->
## 3. Moving through dates

<!-- DIAGRAM:navigation -->

**Mouse or touchpad:** use the arrow controls next to the month and year labels. Scrolling over the date grid browses the preceding or following month/period. Select a day to make it the current selection; select the Today heading to return to the current date.

**Keyboard navigation inside the focused date grid:**

- **Left / Right** - move the selected date back or forward one day.
- **Up / Down** - move backward or forward seven days.
- **Page Up / Page Down** - previous or next month/period.
- **Shift + Page Up / Shift + Page Down** - previous or next year.
- **Home / End** - first or last day of the displayed week.

**Global shortcut:** **Super+C** is the initial **Show calendar** binding. It opens the popup; you can change it in Calendar's settings. A system-wide shortcut conflict may prevent a binding from taking effect.

**Calendar-specific periods.** When a separately installed calendar resource is selected through shared settings, the period and year controls follow that resource's definitions. Calendar does not reinterpret every system as Gregorian months.

<!-- PAGE -->
## 4. Calendar applet settings

<!-- DIAGRAM:settings -->

Open Calendar's configuration from Cinnamon **System Settings > Applets**. The current settings schema contains exactly four user controls in two sections:

**Display: Theme.** **Follow system** (default) uses the current Cinnamon appearance. **Day** and **Night** override the popup's appearance without changing the whole desktop theme.

**Display: Show calendar events.** Enabled by default. When enabled and the desktop event service has a usable calendar, Calendar shows an events agenda and event markers. Turning this off does not delete events or change your event accounts.

**Display: Show week numbers in calendar.** Disabled by default. When enabled, the date grid includes a week-number column. First day of week remains a Cinnamon/locale preference, not an applet-owned selector.

**Keyboard shortcuts: Show calendar.** Default **Super+C**. Rebind it using Cinnamon's keybinding control. This changes the shortcut, not the panel clock or date model.

**Not in this settings page:** the calendar system, clock system, seconds and geographic position. Those are shared temporal preferences and, when supported, belong under **System Settings > Date & Time**. The applet deliberately does not duplicate their controls.

<!-- PAGE -->
## 5. Events and the desktop calendar

Calendar displays events supplied by Cinnamon's event infrastructure. It receives calendar availability and event data through Cinnamon CalendarServer and its configured backend. Calendar itself is **not** an event database, appointment editor or independent background event daemon.

**To view events:** enable **Show calendar events**, open Calendar and select a date. The agenda on the left follows that date. Entries can include a title, time and current/next-event cues. Longer agendas are scrollable.

**To open an event:** select the entry. When the configured desktop calendar application is installed, Calendar launches the selected event or date in that application. The precise launcher depends on Cinnamon's configured calendar backend (GNOME Calendar or Clockenstein, where supported).

**No Events** does not necessarily mean an error: the selected day may have no appointments. If the agenda is absent, either the event option is off or the service reports no usable calendars. If the event service is temporarily unavailable, Calendar reconnects when it returns.

<!-- DIAGRAM:events -->

**Important boundary.** Creating, editing, synchronising and deleting appointments occur in your calendar application/service, not in this panel applet.

<!-- PAGE -->
## 6. System time, alternative resources and location

<!-- DIAGRAM:ownership -->

There are **three different settings owners**. Calendar's own configuration controls appearance, agenda visibility, week numbers and its shortcut. Cinnamon controls ordinary date/time display preferences, locale, time zone and first day of week. Where installed, Infiltrator **System Settings > Date & Time** writes the shared temporal policy, including a selected clock resource, selected calendar resource, seconds and location.

A valid saved shared policy continues to apply even if the optional System Settings interface is later removed. Without a valid policy, Calendar follows Cinnamon's conventional clock/date settings.

**Independent resources.** Calendar consumes installed providers but does not install, implement or document every clock or calendar system. Each clock or calendar package installs separately under `/System/Resources/Clocks` or `/System/Resources/Calendars` and owns its own PDF. A missing or invalid provider is not silently replaced by an invented equivalent.

**Location-dependent display.** Some independent clock resources need latitude/longitude. Configure location in the shared Date & Time settings. `N/A LOC` means the selected mode requires a location and none is valid. Calendar does not assume Greenwich. At high latitudes some astronomical events may be genuinely undefined.

<!-- PAGE -->
## 7. Troubleshooting

**The applet is not in the panel.** Open Cinnamon's Applets manager, verify that **Calendar** has been added to the desired panel and that the package is installed. This application does not remove Cinnamon's stock applet automatically.

**The popup opens but the events area is missing.** Check Calendar configuration > **Show calendar events**. Ensure at least one usable calendar is configured in the desktop calendar backend. The area is hidden if the service reports no calendars.

**A day says No Events.** Check the date selection, then inspect that date directly in the desktop calendar app. The panel applet does not create entries.

**The shortcut does nothing.** Check Calendar configuration > **Show calendar**, including conflicts with existing Cinnamon or application shortcuts. You can still use the panel clock.

**The dates or time look conventional rather than alternative.** Check the active Cinnamon locale/time zone first. Then inspect any valid shared policy under System Settings > Date & Time. An uninstalled independent resource cannot provide an alternative display.

**The grid says Calendar unavailable.** A selected resource may be missing, invalid or outside the model's supported domain. Check its independently installed package; use that provider's manual for resource-specific behaviour.

**The clock reads N/A LOC.** Set a valid geographic position for a location-dependent clock through shared Date & Time settings; the application will not guess a location.

**An upgrade causes the applet to refuse to load.** Calendar deliberately rejects mismatched applet and native-library versions. Verify that the installed application package and its native library match; reinstall the matching release rather than mixing files from different versions.

<!-- PAGE -->
## 8. Quick reference and boundaries

**Open / close:** select the Calendar panel clock.

**Go to today:** select the day/date heading above the grid.

**Select a date:** select a day cell; the agenda follows it.

**Browse:** month/year arrows, scrolling over the grid, or keyboard navigation.

**Appearance and applet controls:** Cinnamon Applets manager > Calendar settings.

**Shared date/time policy:** System Settings > Date & Time, where the shared component is installed.

**Open About:** popup > About Calendar.

**Installed manual:** `/System/Resources/Documentation/Calendar.pdf`.

**Upstream source:** `https://github.com/Infiltrator-Projects/Calendar`.

**Not included in Calendar:** event account setup, event editing, individual clock algorithms, individual calendar conversion algorithms, resource package installation and resource-specific reference manuals.

**Documentation rule:** this manual describes the application and the controls actually present in its implementation. The individual `Calendar-*` and `Clock-*` resource manuals describe their own behaviour. Engineering architecture, chronology evidence and qualification live in the repository's specialist documents.

---
Copyright 1993-2026 Shannon Smith. Calendar is licensed under GPL-3.0-or-later. Interface diagrams in this guide are explanatory schematics, not captured screenshots; minor visual details depend on the installed Cinnamon theme and version.
