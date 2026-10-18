// SPDX-License-Identifier: GPL-3.0-or-later
/* Independently installed calendar. No Calendar application ABI is exported. */
#include <infiltratr/core.h>
#include <infiltratr/temporal_calendar.h>
#include "calendar-engine-types.h"
#include "calendar-arithmetic.h"
#include "calendar-custom.h"
#include "calendar-swedish.h"
#include "icu-calendar.h"
#include "catalogue.h"
#include <string.h>

typedef struct ResourceCalendar ResourceCalendar;
typedef gint64 (*ResourceNavigateFunc)(const ResourceCalendar *, gint64, gint);
struct ResourceCalendar {
    CalendarCalendarMode mode;
    const gchar *icu_keyword;
    gboolean (*fields_from_jdn)(const ResourceCalendar *,gint64,CalendarCalendarFields *);
    gchar *(*format)(const ResourceCalendar *,gint64,CalendarDatePart);
    gint64 (*period_start)(const ResourceCalendar *,gint64);
    ResourceNavigateFunc add_periods;
    ResourceNavigateFunc add_years;
};
#if defined(RESOURCE_FAMILY_ICU)
static gboolean
icu_fields(const ResourceCalendar *provider,
           gint64 jdn,
           CalendarCalendarFields *fields)
{
    const ResourceCalendar *builtin = provider;

    return calendar_icu_fields_from_jdn(
        builtin->icu_keyword, jdn, fields);
}

static gchar *
icu_format(const ResourceCalendar *provider,
           gint64 jdn,
           CalendarDatePart part)
{
    const ResourceCalendar *builtin = provider;

    return calendar_icu_format(
        builtin->mode, builtin->icu_keyword, jdn, part);
}

static gint64
icu_period_start(const ResourceCalendar *provider,
                 gint64 jdn)
{
    const ResourceCalendar *builtin = provider;

    return calendar_icu_month_start(builtin->icu_keyword, jdn);
}

static gint64
icu_add_periods(const ResourceCalendar *provider,
                gint64 jdn,
                gint amount)
{
    const ResourceCalendar *builtin = provider;

    return calendar_icu_add_months(
        builtin->icu_keyword, jdn, amount);
}

static gint64
icu_add_years(const ResourceCalendar *provider,
              gint64 jdn,
              gint amount)
{
    const ResourceCalendar *builtin = provider;

    return calendar_icu_add_years(
        builtin->icu_keyword, jdn, amount);
}

#endif

#if defined(RESOURCE_FAMILY_ARITHMETIC)
static gchar *
icu_format(const ResourceCalendar *provider,
           gint64 jdn,
           CalendarDatePart part)
{
    const ResourceCalendar *builtin = provider;

    return calendar_icu_format(
        builtin->mode, builtin->icu_keyword, jdn, part);
}

static gboolean
arithmetic_fields(const ResourceCalendar *provider,
                  gint64 jdn,
                  CalendarCalendarFields *fields)
{
    const ResourceCalendar *builtin = provider;

    return calendar_arithmetic_fields_from_jdn(
        builtin->mode, jdn, fields);
}

static gint64
arithmetic_period_start(const ResourceCalendar *provider,
                        gint64 jdn)
{
    const ResourceCalendar *builtin = provider;

    return calendar_arithmetic_month_start(builtin->mode, jdn);
}

static gint64
arithmetic_add_periods(const ResourceCalendar *provider,
                       gint64 jdn,
                       gint amount)
{
    const ResourceCalendar *builtin = provider;

    return calendar_arithmetic_add_months(
        builtin->mode, jdn, amount);
}

static gint64
arithmetic_add_years(const ResourceCalendar *provider,
                     gint64 jdn,
                     gint amount)
{
    const ResourceCalendar *builtin = provider;

    return calendar_arithmetic_add_years(
        builtin->mode, jdn, amount);
}

#endif

#if defined(RESOURCE_FAMILY_CUSTOM)
static gboolean
custom_fields(const ResourceCalendar *provider,
              gint64 jdn,
              CalendarCalendarFields *fields)
{
    const ResourceCalendar *builtin = provider;

    calendar_custom_fields_from_jdn(builtin->mode, jdn, fields);
    return TRUE;
}

static gchar *
custom_format(const ResourceCalendar *provider,
              gint64 jdn,
              CalendarDatePart part)
{
    const ResourceCalendar *builtin = provider;
    CalendarCalendarFields fields;

    calendar_custom_fields_from_jdn(builtin->mode, jdn, &fields);
    return calendar_custom_format(builtin->mode, &fields, part);
}

static gint64
custom_period_start(const ResourceCalendar *provider,
                    gint64 jdn)
{
    const ResourceCalendar *builtin = provider;

    return calendar_custom_month_start(builtin->mode, jdn);
}

static gint64
custom_add_periods(const ResourceCalendar *provider,
                   gint64 jdn,
                   gint amount)
{
    const ResourceCalendar *builtin = provider;

    return calendar_custom_add_months(builtin->mode, jdn, amount);
}

static gint64
custom_add_years(const ResourceCalendar *provider,
                 gint64 jdn,
                 gint amount)
{
    const ResourceCalendar *builtin = provider;

    return calendar_custom_add_years(builtin->mode, jdn, amount);
}

#endif

#if defined(RESOURCE_FAMILY_SWEDISH)
static gboolean
swedish_fields(const ResourceCalendar *provider,
               gint64 jdn,
               CalendarCalendarFields *fields)
{
    (void)provider;
    calendar_swedish_fields_from_jdn(jdn, fields);
    return TRUE;
}

static gchar *
swedish_format(const ResourceCalendar *provider,
               gint64 jdn,
               CalendarDatePart part)
{
    CalendarCalendarFields fields;

    (void)provider;
    calendar_swedish_fields_from_jdn(jdn, &fields);
    return calendar_swedish_format(&fields, part);
}

static gint64
swedish_period_start(const ResourceCalendar *provider,
                     gint64 jdn)
{
    (void)provider;
    return calendar_swedish_month_start(jdn);
}

static gint64
swedish_add_periods(const ResourceCalendar *provider,
                    gint64 jdn,
                    gint amount)
{
    (void)provider;
    return calendar_swedish_add_months(jdn, amount);
}

static gint64
swedish_add_years(const ResourceCalendar *provider,
                  gint64 jdn,
                  gint amount)
{
    (void)provider;
    return calendar_swedish_add_years(jdn, amount);
}

#endif

#if defined(RESOURCE_FAMILY_ICU)
#define RESOURCE_OPERATIONS icu_fields, icu_format, icu_period_start, icu_add_periods, icu_add_years
#elif defined(RESOURCE_FAMILY_ARITHMETIC)
#define RESOURCE_OPERATIONS arithmetic_fields, icu_format, arithmetic_period_start, arithmetic_add_periods, arithmetic_add_years
#elif defined(RESOURCE_FAMILY_CUSTOM)
#define RESOURCE_OPERATIONS custom_fields, custom_format, custom_period_start, custom_add_periods, custom_add_years
#else
#define RESOURCE_OPERATIONS swedish_fields, swedish_format, swedish_period_start, swedish_add_periods, swedish_add_years
#endif
static const ResourceCalendar native = {
    RESOURCE_MODE, RESOURCE_KEYWORD, RESOURCE_OPERATIONS
};
static bool fields(const InfiltratrTemporalCalendarProvider *self,
    int64_t jdn, InfiltratrTemporalCalendarFields *result)
{
    CalendarCalendarFields value;
    (void)self;
    if (result == NULL || !native.fields_from_jdn(&native,jdn,&value))
        return false;
    *result = (InfiltratrTemporalCalendarFields){
        value.year, value.month, value.day, value.auxiliary, value.special != FALSE };
    return true;
}
static bool format(const InfiltratrTemporalCalendarProvider *self,
    int64_t jdn, InfiltratrTemporalCalendarDatePart part,
    char *buffer, size_t capacity, size_t *length)
{
    gchar *text;
    (void)self;
    if (buffer == NULL || capacity == 0U ||
        part < INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_DAY ||
        part > INFILTRATR_TEMPORAL_CALENDAR_DATE_PART_FULL) return false;
    text = native.format(&native, jdn, (CalendarDatePart)part);
    if (text == NULL || *text == '\0') { g_free(text); return false; }
    const size_t size = strlen(text);
    if (size >= capacity) { g_free(text); return false; }
    memcpy(buffer,text,size+1U);
    if (length != NULL) *length=size;
    g_free(text);
    return true;
}
static bool start(const InfiltratrTemporalCalendarProvider *self,
    int64_t jdn, int64_t *result)
{
    CalendarCalendarFields value;
    if (result == NULL || !fields(self,jdn,&(InfiltratrTemporalCalendarFields){0}))
        return false;
    const int64_t candidate=native.period_start(&native,jdn);
    if (!native.fields_from_jdn(&native,candidate,&value)) return false;
    *result=candidate;
    return true;
}
static bool navigate(const InfiltratrTemporalCalendarProvider *self,
    int64_t jdn,int32_t amount,int64_t *result,ResourceNavigateFunc operation)
{
    CalendarCalendarFields value;
    if (result == NULL || !fields(self,jdn,&(InfiltratrTemporalCalendarFields){0}))
        return false;
    const int64_t candidate=operation(&native,jdn,amount);
    if (!native.fields_from_jdn(&native,candidate,&value)) return false;
    *result=candidate;
    return true;
}
static bool periods(const InfiltratrTemporalCalendarProvider *self,
    int64_t jdn,int32_t amount,int64_t *result)
{ return navigate(self,jdn,amount,result,native.add_periods); }
static bool years(const InfiltratrTemporalCalendarProvider *self,
    int64_t jdn,int32_t amount,int64_t *result)
{ return navigate(self,jdn,amount,result,native.add_years); }
static const InfiltratrTemporalResourceMetadata metadata = {
    sizeof(InfiltratrTemporalResourceMetadata),
    INFILTRATR_TEMPORAL_RESOURCE_METADATA_ABI,
    INFILTRATR_TEMPORAL_RESOURCE_CALENDAR,RESOURCE_ID,RESOURCE_NAME,false,false,false
};
static const InfiltratrTemporalCalendarProvider provider = {
    sizeof(InfiltratrTemporalCalendarProvider),
    INFILTRATR_TEMPORAL_CALENDAR_PROVIDER_ABI,RESOURCE_ID,
    fields,format,start,periods,years
};
#if defined(_WIN32)
#define RESOURCE_EXPORT __declspec(dllexport)
#else
#define RESOURCE_EXPORT __attribute__((visibility("default")))
#endif
RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *infiltratr_temporal_resource_metadata_v1(void);
RESOURCE_EXPORT const InfiltratrTemporalCalendarProvider *infiltratr_temporal_calendar_provider_v1(void);
RESOURCE_EXPORT const InfiltratrTemporalResourceMetadata *infiltratr_temporal_resource_metadata_v1(void) { return &metadata; }
RESOURCE_EXPORT const InfiltratrTemporalCalendarProvider *infiltratr_temporal_calendar_provider_v1(void) { return &provider; }
