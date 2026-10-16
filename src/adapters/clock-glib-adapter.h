// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_CLOCK_GLIB_ADAPTER_H
#define CALENDAR_CLOCK_GLIB_ADAPTER_H

#include "clock-engine.h"

void calendar_clock_glib_interfaces(
    CalendarClockTimeSource *time_source,
    CalendarClockScheduler *scheduler);

#endif
