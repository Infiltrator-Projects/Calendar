# Calendar

## A clock and calendar for Cinnamon

Calendar is the Infiltrator panel clock and calendar for the Cinnamon desktop. It shows the time in the panel, opens a calendar and event view, and can use independently installed calendar and clock systems.

Calendar works alongside Cinnamon's own Calendar applet. It does not install a separate project daemon or autostart service.

## Getting started

1. Add Calendar to a Cinnamon panel through Cinnamon's applet manager.
2. Select the panel clock to open the calendar. Use the previous and next controls to browse, and select a day to inspect it.
3. Open the applet's configuration for its display, event and shortcut options. The default Show calendar shortcut is Super+C; it can be changed in the applet settings.

The panel clock, calendar grid and event view follow the desktop's current language and time zone. The available date and time forms depend on the independently installed resource packages and the current system policy.

## Calendar settings

- Theme: Follow system, Day or Night.
- Show calendar events: display events supplied by the desktop calendar service.
- Show week numbers in calendar: add week numbers to the calendar view.
- Show calendar: set the keyboard shortcut that opens the applet.

These are Calendar-specific settings. Clock system, calendar system, seconds and geographic location are shared temporal settings, managed by System Settings when that component is installed.

## Calendars and clocks

Calendar can use 31 independently packaged calendar systems and 23 explicit clock modes. Examples include Gregorian, Julian, Hebrew, Chinese, Islamic and Persian calendars, and 12-hour, 24-hour, decimal, binary, solar and sidereal clocks.

The providers are shared system capabilities: Calendar, Files and System Settings can consume them. Each provider has its own package and user guide. Removing the Calendar app does not remove the independently installed providers.

When no valid shared temporal policy exists, Calendar follows the normal Cinnamon and locale settings for conventional clock and date display. A valid saved shared policy remains authoritative even if the System Settings interface is removed.

## Events

Calendar receives event information from Cinnamon CalendarServer and Evolution Data Server. The applet displays available events when Show calendar events is enabled. If the service restarts, Calendar reconnects; it does not run its own separate event daemon.

## Location and historical models

Some solar, seasonal and historical clock modes need a geographic location. Set it in System Settings > Date & Time. Calendar displays N/A LOC when a required location is unavailable; it does not silently substitute Greenwich.

Historical and astronomical modes are defined computational models. They should not be taken as a claim that every historic place used the same convention. Some solar events can be unavailable at high latitudes.

## If something is missing

- Missing clock or calendar choice: check that its independent provider package is installed and valid.
- N/A LOC: check the geographic location in System Settings > Date & Time.
- Missing events: check that Show calendar events is enabled and that CalendarServer and Evolution Data Server are available.
- Unexpected conventional time or date: check the Cinnamon time zone, locale and clock settings, then any saved shared temporal policy.
- Unexpected typeface: Calendar requests the project's preferred family names when available and otherwise uses the desktop's normal font fallback.

## Installed files and further information

The Calendar application library is installed as libcalendar.so.0.0.0 in the system's multiarch library directory. Calendar and clock providers are installed separately under /System/Resources/Calendars and /System/Resources/Clocks. This guide is installed as /System/Resources/Documentation/Calendar.pdf.

The project README describes the current release and capabilities. MODELS.md records the calendar and clock models; ARCHITECTURE.md and QUALIFICATION.md describe implementation boundaries and evidence. These project documents remain authoritative when this guide summarises them.

Project: https://github.com/Infiltrator-Projects/Calendar
