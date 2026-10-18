// SPDX-License-Identifier: GPL-3.0-or-later
/* Julian chronology is owned exclusively by calendar resources. */
#include "julian-day.h"

gint
calendar_julian_month_length(gint64 year,
                                  gint month)
{
    static const gint lengths[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && calendar_positive_modulo(year, 4) == 0)
        return 29;
    return lengths[month - 1];
}

gint64
calendar_julian_to_jdn(gint64 year,
                            gint month,
                            gint day)
{
    const gint64 a = calendar_floor_divide(14 - month, 12);
    const gint64 y = calendar_i64_subtract_saturating(
        calendar_i64_add_saturating(year, 4800), a);
    const gint64 m = calendar_i64_subtract_saturating(
        calendar_i64_add_saturating(
            month, calendar_i64_multiply_saturating(12, a)),
        3);
    gint64 result = day;

    result = calendar_i64_add_saturating(
        result,
        calendar_floor_divide(
            calendar_i64_add_saturating(
                calendar_i64_multiply_saturating(153, m), 2),
            5));
    result = calendar_i64_add_saturating(
        result, calendar_i64_multiply_saturating(365, y));
    result = calendar_i64_add_saturating(
        result, calendar_floor_divide(y, 4));
    return calendar_i64_subtract_saturating(result, 32083);
}

void
calendar_jdn_to_julian(gint64 jdn,
                            gint *year,
                            gint *month,
                            gint *day)
{
    const gint64 c = calendar_i64_add_saturating(jdn, 32082);
    const gint64 d = calendar_floor_divide(
        calendar_i64_add_saturating(
            calendar_i64_multiply_saturating(4, c), 3),
        1461);
    const gint64 e = calendar_i64_subtract_saturating(
        c,
        calendar_floor_divide(
            calendar_i64_multiply_saturating(1461, d), 4));
    const gint64 m = calendar_floor_divide(
        calendar_i64_add_saturating(
            calendar_i64_multiply_saturating(5, e), 2),
        153);
    const gint64 month_adjustment =
        calendar_i64_multiply_saturating(
            12, calendar_floor_divide(m, 10));
    gint64 computed_year;

    g_return_if_fail(year != NULL);
    g_return_if_fail(month != NULL);
    g_return_if_fail(day != NULL);

    *day = (gint)calendar_i64_add_saturating(
        calendar_i64_subtract_saturating(
            e,
            calendar_floor_divide(
                calendar_i64_add_saturating(
                    calendar_i64_multiply_saturating(153, m), 2),
                5)),
        1);
    *month = (gint)calendar_i64_subtract_saturating(
        calendar_i64_add_saturating(m, 3), month_adjustment);

    computed_year = calendar_i64_subtract_saturating(d, 4800);
    computed_year = calendar_i64_add_saturating(
        computed_year, calendar_floor_divide(m, 10));
    *year = (gint)computed_year;
}

