// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

#ifndef CALENDAR_EVENT_STORE_PRIVATE_H
#define CALENDAR_EVENT_STORE_PRIVATE_H

#include "event-core.h"
#include "event-store.h"

CalendarEventIndex *calendar_event_store_get_index(
    CalendarEventStore *self);

#endif
