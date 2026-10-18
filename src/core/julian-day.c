// SPDX-License-Identifier: GPL-3.0-or-later
/* ABI adapter only: Common owns every civil coordinate calculation. */
#include "julian-day.h"
#include <infiltratr/temporal.h>
gboolean calendar_gregorian_is_leap(gint64 y) { return infiltratr_temporal_gregorian_is_leap(y); }
gint calendar_gregorian_month_length(gint64 y, gint m) { return infiltratr_temporal_gregorian_month_length(y, m); }
gboolean calendar_gregorian_date_is_valid(gint y, gint m, gint d) { return infiltratr_temporal_gregorian_date_valid(y, m, d); }
gint64 calendar_gregorian_to_jdn(gint64 y, gint m, gint d) { return infiltratr_temporal_gregorian_to_jdn(y, m, d); }
void calendar_jdn_to_gregorian(gint64 jdn, gint *y, gint *m, gint *d) { infiltratr_temporal_jdn_to_gregorian(jdn, y, m, d); }
gint calendar_gregorian_day_of_year(gint y, gint m, gint d) { return infiltratr_temporal_gregorian_day_of_year(y, m, d); }
gint calendar_iso_weekday(gint64 jdn) { return infiltratr_temporal_iso_weekday(jdn); }
gchar *calendar_format_iso_date(gint64 jdn)
{
    gint y, m, d;
    infiltratr_temporal_jdn_to_gregorian(jdn, &y, &m, &d);
    return g_strdup_printf("%04d-%02d-%02d", y, m, d);
}
