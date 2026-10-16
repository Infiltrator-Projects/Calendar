// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * GObject lifetime facade for the platform-neutral event index.
 *
 * CalendarServer tuple parsing and GVariant snapshot encoding are isolated in
 * event-gvariant-adapter.c. This object owns the GObject lifetime and the
 * compatibility API for `::`-separated removal identifiers.
 */

#include "event-store-private.h"

#include <infiltratr/arithmetic.h>
#include <string.h>

struct _CalendarEventStore
{
    GObject parent_instance;

    CalendarEventIndex *index;
};

/* GLib's type-registration macro contains an intentional pointer probe. */
// NOLINTNEXTLINE(performance-no-int-to-ptr)
G_DEFINE_TYPE(CalendarEventStore,
              calendar_event_store,
              G_TYPE_OBJECT)

static void
calendar_event_store_dispose(GObject *object)
{
    CalendarEventStore *self = CALENDAR_EVENT_STORE(object);

    calendar_event_index_free(self->index);
    self->index = NULL;
    G_OBJECT_CLASS(calendar_event_store_parent_class)->dispose(object); // NOLINT(bugprone-casting-through-void)
}

static void
calendar_event_store_class_init(CalendarEventStoreClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass); // NOLINT(bugprone-casting-through-void)

    object_class->dispose = calendar_event_store_dispose;
}

static void
calendar_event_store_init(CalendarEventStore *self)
{
    self->index = calendar_event_index_new();
}

CalendarEventStore *
calendar_event_store_new(void)
{
    return g_object_new(CALENDAR_TYPE_EVENT_STORE, NULL);
}

CalendarEventIndex *
calendar_event_store_get_index(CalendarEventStore *self)
{
    g_return_val_if_fail(CALENDAR_IS_EVENT_STORE(self), NULL);
    return self->index;
}

static gboolean
serialized_event_ids_are_valid(const gchar *serialized_ids)
{
    const gchar *cursor = serialized_ids;

    while (cursor[0] != '\0')
    {
        const gchar *separator = strstr(cursor, "::");
        const gsize length = separator != NULL
            ? (gsize)(separator - cursor)
            : strlen(cursor);

        if (length > CALENDAR_EVENT_MAX_ID_BYTES ||
            (length > 0 &&
             !g_utf8_validate(cursor, (gssize)length, NULL)))
        {
            return FALSE;
        }

        if (separator == NULL)
            break;
        cursor = separator + 2;
    }
    return TRUE;
}

gboolean
calendar_event_store_remove(CalendarEventStore *self,
                                 const gchar *serialized_ids)
{
    const gchar *cursor;
    gboolean changed = FALSE;

    g_return_val_if_fail(CALENDAR_IS_EVENT_STORE(self), FALSE);
    if (serialized_ids == NULL || serialized_ids[0] == '\0' ||
        !serialized_event_ids_are_valid(serialized_ids))
    {
        return FALSE;
    }

    /*
     * Validate the entire external payload before mutating the index, then
     * materialize at most one bounded identifier at a time.  CalendarServer
     * may legally remove many events in one notification; memory use therefore
     * scales with the identifier contract rather than with message size.
     */
    cursor = serialized_ids;
    while (cursor[0] != '\0')
    {
        const gchar *separator = strstr(cursor, "::");
        const gsize length = separator != NULL
            ? (gsize)(separator - cursor)
            : strlen(cursor);

        if (length > 0)
        {
            g_autofree gchar *id = g_strndup(cursor, length);

            changed |= calendar_event_index_remove(self->index, id);
        }

        if (separator == NULL)
            break;
        cursor = separator + 2;
    }
    return changed;
}

gboolean
calendar_event_store_cull(CalendarEventStore *self,
                               gint64 minimum_update_timestamp)
{
    g_return_val_if_fail(CALENDAR_IS_EVENT_STORE(self), FALSE);
    return calendar_event_index_cull(self->index,
                                          minimum_update_timestamp);
}

void
calendar_event_store_clear(CalendarEventStore *self)
{
    g_return_if_fail(CALENDAR_IS_EVENT_STORE(self));
    calendar_event_index_clear(self->index);
}

gboolean
calendar_event_store_refresh_timezone(CalendarEventStore *self)
{
    g_return_val_if_fail(CALENDAR_IS_EVENT_STORE(self), FALSE);
    return calendar_event_index_refresh_timezone(self->index);
}

gchar **
calendar_event_store_get_colors(CalendarEventStore *self,
                                     gint64 local_day_unix,
                                     gint64 now_unix)
{
    g_return_val_if_fail(CALENDAR_IS_EVENT_STORE(self), NULL);

    /*
     * Month-grid dots need only stable colours. Preserve the agenda's ordering
     * semantics while avoiding a deep-copied event snapshot for every cell.
     */
    return calendar_event_index_colors(self->index,
                                            local_day_unix,
                                            now_unix);
}
