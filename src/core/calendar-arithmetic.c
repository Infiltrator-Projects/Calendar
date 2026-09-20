// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned arithmetic calendar engines.
 *
 * These rules intentionally replace ICU only where the calendar itself is a
 * deterministic arithmetic system. ICU remains the locale-formatting authority
 * for these providers until Calendar also owns the corresponding locale data.
 *
 * Epochs and arithmetic follow the same civil-date/JDN convention used by the
 * rest of Calendar: an integral JDN identifies a civil date at midnight.
 */

#include "calendar-arithmetic.h"

#include "julian-day.h"

enum
{
    CALENDAR_PLUS_COPTIC_EPOCH_JDN = 1825030,
    CALENDAR_PLUS_ETHIOPIC_EPOCH_JDN = 1724221,
    CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN = 1948440,
    CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN = 1948439,
    CALENDAR_PLUS_PERSIAN_EPOCH_JDN = 1948320
};

#define ETHIOPIC_AMETE_ALEM_OFFSET G_GINT64_CONSTANT(5500)
#define INDIAN_GREGORIAN_OFFSET G_GINT64_CONSTANT(78)
#define UMM_AL_QURA_FIRST_YEAR G_GINT64_CONSTANT(1300)
#define UMM_AL_QURA_LAST_YEAR G_GINT64_CONSTANT(1600)
#define UMM_AL_QURA_FIRST_JDN G_GINT64_CONSTANT(2408762)

/*
 * Umm al-Qura month lengths for 1300-1600 AH. Each 12-bit mask stores
 * Muharram in bit 11 through Dhu al-Hijjah in bit 0; one means 30 days and
 * zero means 29. The table is a compact projection of the maintained
 * 1300-1600 data used by the CLDR/ICU-compatible ecosystem. Calendar owns the
 * runtime representation so conversion no longer requires ICU.
 */
static const guint16 umm_al_qura_month_masks[] = {
    0xAAAU, 0xD54U, 0xEC9U, 0x6D4U, 0x6EAU, 0x36CU, 0xAADU, 0x555U, 0x6A9U, 0x792U,
    0xBA9U, 0x5D4U, 0xADAU, 0x55CU, 0xD2DU, 0x695U, 0x74AU, 0xB54U, 0xB6AU, 0x5ADU,
    0x4AEU, 0xA4FU, 0x517U, 0x68BU, 0x6A5U, 0xAD5U, 0x2D6U, 0x95BU, 0x49DU, 0xA4DU,
    0xD26U, 0xD95U, 0x5ACU, 0x9B6U, 0x2BAU, 0xA5BU, 0x52BU, 0xA95U, 0x6CAU, 0xAE9U,
    0x2F4U, 0x976U, 0x2B6U, 0x956U, 0xACAU, 0xBA4U, 0xBD2U, 0x5D9U, 0x2DCU, 0x96DU,
    0x54DU, 0xAA5U, 0xB52U, 0xBA5U, 0x5B4U, 0x9B6U, 0x557U, 0x297U, 0x54BU, 0x6A3U,
    0x752U, 0xB65U, 0x56AU, 0xAABU, 0x52BU, 0xC95U, 0xD4AU, 0xDA5U, 0x5CAU, 0xAD6U,
    0x957U, 0x4ABU, 0x94BU, 0xAA5U, 0xB52U, 0xB6AU, 0x575U, 0x276U, 0x8B7U, 0x45BU,
    0x555U, 0x5A9U, 0x5B4U, 0x9DAU, 0x4DDU, 0x26EU, 0x936U, 0xAAAU, 0xD54U, 0xDB2U,
    0x5D5U, 0x2DAU, 0x95BU, 0x4ABU, 0xA55U, 0xB49U, 0xB64U, 0xB71U, 0x5B4U, 0xAB5U,
    0xA55U, 0xD25U, 0xE92U, 0xEC9U, 0x6D4U, 0xAE9U, 0x96BU, 0x4ABU, 0xA93U, 0xD49U,
    0xDA4U, 0xDB2U, 0xAB9U, 0x4BAU, 0xA5BU, 0x52BU, 0xA95U, 0xB2AU, 0xB55U, 0x55CU,
    0x4BDU, 0x23DU, 0x91DU, 0xA95U, 0xB4AU, 0xB5AU, 0x56DU, 0x2B6U, 0x93BU, 0x49BU,
    0x655U, 0x6A9U, 0x754U, 0xB6AU, 0x56CU, 0xAADU, 0x555U, 0xB29U, 0xB92U, 0xBA9U,
    0x5D4U, 0xADAU, 0x55AU, 0xAABU, 0x595U, 0x749U, 0x764U, 0xBAAU, 0x5B5U, 0x2B6U,
    0xA56U, 0xE4DU, 0xB25U, 0xB52U, 0xB6AU, 0x5ADU, 0x2AEU, 0x92FU, 0x497U, 0x64BU,
    0x6A5U, 0x6ACU, 0xAD6U, 0x55DU, 0x49DU, 0xA4DU, 0xD16U, 0xD95U, 0x5AAU, 0x5B5U,
    0x2DAU, 0x95BU, 0x4ADU, 0x595U, 0x6CAU, 0x6E4U, 0xAEAU, 0x4F5U, 0x2B6U, 0x956U,
    0xAAAU, 0xB54U, 0xBD2U, 0x5D9U, 0x2EAU, 0x96DU, 0x4ADU, 0xA95U, 0xB4AU, 0xBA5U,
    0x5B2U, 0x9B5U, 0x4D6U, 0xA97U, 0x547U, 0x693U, 0x749U, 0xB55U, 0x56AU, 0xA6BU,
    0x52BU, 0xA8BU, 0xD46U, 0xDA3U, 0x5CAU, 0xAD6U, 0x4DBU, 0x26BU, 0x94BU, 0xAA5U,
    0xB52U, 0xB69U, 0x575U, 0x176U, 0x8B7U, 0x25BU, 0x52BU, 0x565U, 0x5B4U, 0x9DAU,
    0x4EDU, 0x16DU, 0x8B6U, 0xAA6U, 0xD52U, 0xDA9U, 0x5D4U, 0xADAU, 0x95BU, 0x4ABU,
    0x653U, 0x729U, 0x762U, 0xBA9U, 0x5B2U, 0xAB5U, 0x555U, 0xB25U, 0xD92U, 0xEC9U,
    0x6D2U, 0xAE9U, 0x56BU, 0x4ABU, 0xA55U, 0xD29U, 0xD54U, 0xDAAU, 0x9B5U, 0x4BAU,
    0xA3BU, 0x49BU, 0xA4DU, 0xAAAU, 0xAD5U, 0x2DAU, 0x95DU, 0x45EU, 0xA2EU, 0xC9AU,
    0xD55U, 0x6B2U, 0x6B9U, 0x4BAU, 0xA5DU, 0x52DU, 0xA95U, 0xB52U, 0xBA8U, 0xBB4U,
    0x5B9U, 0x2DAU, 0x95AU, 0xB4AU, 0xDA4U, 0xED1U, 0x6E8U, 0xB6AU, 0x56DU, 0x535U,
    0x695U, 0xD4AU, 0xDA8U, 0xDD4U, 0x6DAU, 0x55BU, 0x29DU, 0x62BU, 0xB15U, 0xB4AU,
    0xB95U, 0x5AAU, 0xAAEU, 0x92EU, 0xC8FU, 0x527U, 0x695U, 0x6AAU, 0xAD6U, 0x55DU,
    0x29DU
};

/* Cumulative days from 1300-01-01 to each year boundary, plus the 1601 end. */
static const guint32 umm_al_qura_year_offsets[] = {
    0U, 354U, 708U, 1063U, 1417U, 1772U, 2126U, 2481U, 2835U, 3189U,
    3543U, 3898U, 4252U, 4607U, 4961U, 5316U, 5670U, 6024U, 6378U, 6733U,
    7088U, 7442U, 7797U, 8151U, 8505U, 8859U, 9214U, 9568U, 9923U, 10277U,
    10631U, 10985U, 11340U, 11694U, 12049U, 12403U, 12758U, 13112U, 13466U, 13820U,
    14175U, 14529U, 14884U, 15238U, 15592U, 15946U, 16300U, 16655U, 17010U, 17364U,
    17719U, 18073U, 18427U, 18781U, 19136U, 19490U, 19845U, 20200U, 20554U, 20908U,
    21262U, 21616U, 21971U, 22325U, 22680U, 23034U, 23388U, 23742U, 24097U, 24451U,
    24806U, 25161U, 25515U, 25869U, 26223U, 26577U, 26932U, 27287U, 27641U, 27996U,
    28350U, 28704U, 29058U, 29412U, 29767U, 30122U, 30476U, 30830U, 31184U, 31538U,
    31893U, 32248U, 32602U, 32957U, 33311U, 33665U, 34019U, 34373U, 34728U, 35082U,
    35437U, 35791U, 36145U, 36499U, 36854U, 37208U, 37563U, 37918U, 38272U, 38626U,
    38980U, 39334U, 39689U, 40044U, 40398U, 40753U, 41107U, 41461U, 41815U, 42170U,
    42524U, 42879U, 43233U, 43587U, 43941U, 44295U, 44650U, 45005U, 45359U, 45714U,
    46068U, 46422U, 46776U, 47130U, 47485U, 47839U, 48194U, 48548U, 48902U, 49256U,
    49611U, 49965U, 50320U, 50674U, 51029U, 51383U, 51737U, 52091U, 52446U, 52801U,
    53155U, 53509U, 53864U, 54218U, 54572U, 54927U, 55282U, 55636U, 55991U, 56345U,
    56699U, 57053U, 57407U, 57762U, 58117U, 58471U, 58825U, 59179U, 59534U, 59888U,
    60243U, 60597U, 60952U, 61306U, 61660U, 62014U, 62368U, 62723U, 63078U, 63432U,
    63786U, 64140U, 64494U, 64849U, 65204U, 65558U, 65913U, 66267U, 66621U, 66975U,
    67330U, 67684U, 68039U, 68393U, 68748U, 69102U, 69456U, 69810U, 70165U, 70519U,
    70874U, 71228U, 71582U, 71936U, 72291U, 72645U, 73000U, 73355U, 73709U, 74063U,
    74417U, 74771U, 75126U, 75481U, 75835U, 76190U, 76544U, 76898U, 77252U, 77606U,
    77961U, 78316U, 78670U, 79024U, 79378U, 79732U, 80087U, 80441U, 80796U, 81151U,
    81505U, 81859U, 82213U, 82567U, 82922U, 83276U, 83631U, 83985U, 84339U, 84693U,
    85048U, 85402U, 85757U, 86112U, 86466U, 86820U, 87174U, 87528U, 87883U, 88238U,
    88592U, 88947U, 89301U, 89655U, 90009U, 90364U, 90718U, 91073U, 91427U, 91781U,
    92135U, 92490U, 92844U, 93199U, 93553U, 93908U, 94262U, 94616U, 94970U, 95324U,
    95679U, 96034U, 96388U, 96742U, 97096U, 97450U, 97805U, 98159U, 98514U, 98869U,
    99223U, 99577U, 99931U, 100285U, 100640U, 100995U, 101350U, 101704U, 102058U, 102412U,
    102766U, 103121U, 103475U, 103830U, 104184U, 104539U, 104893U, 105247U, 105601U, 105956U,
    106311U, 106665U
};

G_STATIC_ASSERT(G_N_ELEMENTS(umm_al_qura_month_masks) == 301);
G_STATIC_ASSERT(G_N_ELEMENTS(umm_al_qura_year_offsets) == 302);

typedef struct
{
    gint code;
    gint start_year;
    gint start_month;
    gint start_day;
} JapaneseEra;

/*
 * CLDR's modern Japanese era boundary data. CLDR 49 / ICU 79 removes the
 * pre-Meiji era sequence, so Calendar owns the stable modern boundary that
 * current ICU itself converges on instead of inheriting version-dependent
 * historical-era tables from the host.
 */
static const JapaneseEra japanese_eras[] = {
    { 232, 1868, 10, 23 }, /* Meiji */
    { 233, 1912, 7, 30 },  /* Taisho */
    { 234, 1926, 12, 25 }, /* Showa */
    { 235, 1989, 1, 8 },   /* Heisei */
    { 236, 2019, 5, 1 }    /* Reiwa */
};

static gboolean
japanese_date_on_or_after(gint year,
                          gint month,
                          gint day,
                          const JapaneseEra *era)
{
    if (year != era->start_year)
        return year > era->start_year;
    if (month != era->start_month)
        return month > era->start_month;
    return day >= era->start_day;
}

static const JapaneseEra *
japanese_era_by_code(gint64 code)
{
    gsize index;

    for (index = 0; index < G_N_ELEMENTS(japanese_eras); index++)
    {
        if (code == japanese_eras[index].code)
            return &japanese_eras[index];
    }
    return NULL;
}

static gint64
japanese_signed_year_from_fields(const CalendarPlusCalendarFields *fields)
{
    const JapaneseEra *era;

    if (fields->auxiliary == 0)
        return calendar_plus_i64_subtract_saturating(1, fields->year);
    if (fields->auxiliary == 1)
        return fields->year;

    era = japanese_era_by_code(fields->auxiliary);
    if (era == NULL)
        return fields->year;

    return calendar_plus_i64_add_saturating(
        era->start_year,
        calendar_plus_i64_subtract_saturating(fields->year, 1));
}

static void
japanese_set_year_for_gregorian(gint64 gregorian_year,
                                CalendarPlusCalendarFields *fields)
{
    gsize index;

    if (gregorian_year < 1)
    {
        fields->year =
            calendar_plus_i64_subtract_saturating(1, gregorian_year);
        fields->auxiliary = 0;
        return;
    }

    fields->year = gregorian_year;
    fields->auxiliary = 1;
    for (index = 0; index < G_N_ELEMENTS(japanese_eras); index++)
    {
        if (gregorian_year < japanese_eras[index].start_year)
            break;
        fields->year = calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                gregorian_year, japanese_eras[index].start_year),
            1);
        fields->auxiliary = japanese_eras[index].code;
    }
}

static void
japanese_from_jdn(gint64 jdn,
                  CalendarPlusCalendarFields *fields)
{
    gint year;
    gint month;
    gint day;
    gsize index;

    calendar_plus_jdn_to_gregorian(jdn, &year, &month, &day);
    fields->year = year >= 1 ? year :
        calendar_plus_i64_subtract_saturating(1, year);
    fields->month = month;
    fields->day = day;
    fields->auxiliary = year >= 1 ? 1 : 0;
    fields->special = FALSE;

    for (index = 0; index < G_N_ELEMENTS(japanese_eras); index++)
    {
        const JapaneseEra *era = &japanese_eras[index];

        if (!japanese_date_on_or_after(year, month, day, era))
            break;

        fields->year = calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(year, era->start_year),
            1);
        fields->auxiliary = era->code;
    }
}

static gboolean
arithmetic_mode_supported(CalendarPlusCalendarMode mode)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return TRUE;
        default:
            return FALSE;
    }
}

static gint64
signed_year_from_fields(CalendarPlusCalendarMode mode,
                        const CalendarPlusCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(1, fields->year) :
                fields->year;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(
                    fields->year, ETHIOPIC_AMETE_ALEM_OFFSET) :
                fields->year;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return calendar_plus_i64_subtract_saturating(
                fields->year, ETHIOPIC_AMETE_ALEM_OFFSET);

        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
            return calendar_plus_i64_subtract_saturating(fields->year, 543);

        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            return fields->auxiliary == 0 ?
                calendar_plus_i64_subtract_saturating(
                    1912, fields->year) :
                calendar_plus_i64_add_saturating(
                    fields->year, 1911);

        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            return japanese_signed_year_from_fields(fields);

        default:
            return fields->year;
    }
}

static void
set_signed_year(CalendarPlusCalendarMode mode,
                gint64 signed_year,
                CalendarPlusCalendarFields *fields)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            if (signed_year >= 1)
            {
                fields->year = signed_year;
                fields->auxiliary = 1;
            }
            else
            {
                fields->year =
                    calendar_plus_i64_subtract_saturating(1, signed_year);
                fields->auxiliary = 0;
            }
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
            if (signed_year >= 1)
            {
                fields->year = signed_year;
                fields->auxiliary = 1;
            }
            else
            {
                fields->year = calendar_plus_i64_add_saturating(
                    signed_year, ETHIOPIC_AMETE_ALEM_OFFSET);
                fields->auxiliary = 0;
            }
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            fields->year = calendar_plus_i64_add_saturating(
                signed_year, ETHIOPIC_AMETE_ALEM_OFFSET);
            fields->auxiliary = 0;
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
            fields->year = calendar_plus_i64_add_saturating(
                signed_year, 543);
            fields->auxiliary = 0;
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            if (signed_year >= 1912)
            {
                fields->year = calendar_plus_i64_subtract_saturating(
                    signed_year, 1911);
                fields->auxiliary = 1;
            }
            else
            {
                fields->year = calendar_plus_i64_subtract_saturating(
                    1912, signed_year);
                fields->auxiliary = 0;
            }
            break;

        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            japanese_set_year_for_gregorian(signed_year, fields);
            break;

        default:
            fields->year = signed_year;
            fields->auxiliary = 0;
            break;
    }
}

static gboolean
islamic_is_leap(gint64 year)
{
    const gint64 numerator = calendar_plus_i64_add_saturating(
        calendar_plus_i64_multiply_saturating(11, year), 14);

    return calendar_plus_positive_modulo(numerator, 30) < 11;
}

static gint
islamic_month_length(gint64 year,
                     gint month)
{
    if (month < 1 || month > 12)
        return 0;
    if (month == 12 && islamic_is_leap(year))
        return 30;
    return (month % 2) == 1 ? 30 : 29;
}

static gint64
islamic_to_jdn(gint64 year,
               gint month,
               gint day,
               gint64 epoch)
{
    const gint64 month_index = month - 1;
    const gint64 month_days = calendar_plus_floor_divide(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(59, month_index), 1),
        2);
    gint64 result = calendar_plus_i64_subtract_saturating(epoch, 1);

    result = calendar_plus_i64_add_saturating(result, day);
    result = calendar_plus_i64_add_saturating(result, month_days);
    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(
            354, calendar_plus_i64_subtract_saturating(year, 1)));
    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                3, calendar_plus_i64_multiply_saturating(11, year)),
            30));
    return result;
}

static void
islamic_from_jdn(gint64 jdn,
                 gint64 epoch,
                 CalendarPlusCalendarFields *fields)
{
    const gint64 year = calendar_plus_floor_divide(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(
                30, calendar_plus_i64_subtract_saturating(jdn, epoch)),
            10646),
        10631);
    gint month = 1;

    while (month < 12 &&
           jdn >= islamic_to_jdn(year, month + 1, 1, epoch))
    {
        month++;
    }

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month,
        .day = (gint)calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                jdn, islamic_to_jdn(year, month, 1, epoch)),
            1),
        .auxiliary = 0,
        .special = FALSE
    };
}

static gint
umm_al_qura_month_length(gint64 year,
                         gint month)
{
    guint16 mask;

    if (year < UMM_AL_QURA_FIRST_YEAR || year > UMM_AL_QURA_LAST_YEAR ||
        month < 1 || month > 12)
    {
        return 0;
    }

    mask = umm_al_qura_month_masks[year - UMM_AL_QURA_FIRST_YEAR];
    return (mask & (1U << (12 - month))) != 0U ? 30 : 29;
}

static gint64
umm_al_qura_to_jdn(gint64 year,
                    gint month,
                    gint day)
{
    gint64 result;
    gint current_month;

    if (year < UMM_AL_QURA_FIRST_YEAR || year > UMM_AL_QURA_LAST_YEAR ||
        month < 1 || month > 12)
    {
        return CALENDAR_PLUS_UNIX_EPOCH_JDN;
    }

    result = calendar_plus_i64_add_saturating(
        UMM_AL_QURA_FIRST_JDN,
        umm_al_qura_year_offsets[year - UMM_AL_QURA_FIRST_YEAR]);
    for (current_month = 1; current_month < month; current_month++)
    {
        result = calendar_plus_i64_add_saturating(
            result, umm_al_qura_month_length(year, current_month));
    }
    return calendar_plus_i64_add_saturating(result, day - 1);
}

static gboolean
umm_al_qura_from_jdn(gint64 jdn,
                     CalendarPlusCalendarFields *fields)
{
    const gint64 day_offset = calendar_plus_i64_subtract_saturating(
        jdn, UMM_AL_QURA_FIRST_JDN);
    gsize low = 0;
    gsize high = G_N_ELEMENTS(umm_al_qura_month_masks);
    gsize year_index;
    gint64 day_of_year;
    gint month = 1;

    if (day_offset < 0 ||
        day_offset >= (gint64)umm_al_qura_year_offsets[
            G_N_ELEMENTS(umm_al_qura_year_offsets) - 1])
    {
        return FALSE;
    }

    while (low + 1 < high)
    {
        const gsize middle = low + (high - low) / 2;

        if ((gint64)umm_al_qura_year_offsets[middle] <= day_offset)
            low = middle;
        else
            high = middle;
    }
    year_index = low;
    day_of_year = calendar_plus_i64_subtract_saturating(
        day_offset, umm_al_qura_year_offsets[year_index]);

    while (month < 12)
    {
        const gint length = umm_al_qura_month_length(
            UMM_AL_QURA_FIRST_YEAR + (gint64)year_index, month);

        if (day_of_year < length)
            break;
        day_of_year = calendar_plus_i64_subtract_saturating(
            day_of_year, length);
        month++;
    }

    *fields = (CalendarPlusCalendarFields){
        .year = UMM_AL_QURA_FIRST_YEAR + (gint64)year_index,
        .month = month,
        .day = (gint)day_of_year + 1,
        .auxiliary = 0,
        .special = FALSE
    };
    return TRUE;
}

enum
{
    HEBREW_HOUR_PARTS = 1080,
    HEBREW_DAY_PARTS = 24 * HEBREW_HOUR_PARTS,
    HEBREW_MONTH_FRACT = 12 * HEBREW_HOUR_PARTS + 793,
    HEBREW_MONTH_PARTS = 29 * HEBREW_DAY_PARTS + HEBREW_MONTH_FRACT,
    HEBREW_BAHARAD = 11 * HEBREW_HOUR_PARTS + 204,
    HEBREW_EPOCH_OFFSET = 347998
};

static const gint hebrew_month_length_table[13][3] = {
    { 30, 30, 30 }, { 29, 29, 30 }, { 29, 30, 30 },
    { 29, 29, 29 }, { 30, 30, 30 }, { 30, 30, 30 },
    { 29, 29, 29 }, { 30, 30, 30 }, { 29, 29, 29 },
    { 30, 30, 30 }, { 29, 29, 29 }, { 30, 30, 30 },
    { 29, 29, 29 }
};

static const gint hebrew_month_start[14][3] = {
    { 0, 0, 0 }, { 30, 30, 30 }, { 59, 59, 60 },
    { 88, 89, 90 }, { 117, 118, 119 }, { 147, 148, 149 },
    { 147, 148, 149 }, { 176, 177, 178 }, { 206, 207, 208 },
    { 235, 236, 237 }, { 265, 266, 267 }, { 294, 295, 296 },
    { 324, 325, 326 }, { 353, 354, 355 }
};

static const gint hebrew_leap_month_start[14][3] = {
    { 0, 0, 0 }, { 30, 30, 30 }, { 59, 59, 60 },
    { 88, 89, 90 }, { 117, 118, 119 }, { 147, 148, 149 },
    { 177, 178, 179 }, { 206, 207, 208 }, { 236, 237, 238 },
    { 265, 266, 267 }, { 295, 296, 297 }, { 324, 325, 326 },
    { 354, 355, 356 }, { 383, 384, 385 }
};

static gboolean
hebrew_is_leap(gint64 year)
{
    return calendar_plus_positive_modulo(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(year, 12), 17),
        19) >= 12;
}

static gint64
hebrew_start_of_year(gint64 year)
{
    const gint64 months = calendar_plus_floor_divide(
        calendar_plus_i64_subtract_saturating(
            calendar_plus_i64_multiply_saturating(235, year), 234),
        19);
    const gint64 total_fraction = calendar_plus_i64_add_saturating(
        calendar_plus_i64_multiply_saturating(
            months, HEBREW_MONTH_FRACT),
        HEBREW_BAHARAD);
    const gint64 fraction_days = calendar_plus_floor_divide(
        total_fraction, HEBREW_DAY_PARTS);
    gint64 day = calendar_plus_i64_add_saturating(
        calendar_plus_i64_multiply_saturating(months, 29),
        fraction_days);
    const gint64 fraction = calendar_plus_i64_subtract_saturating(
        total_fraction,
        calendar_plus_i64_multiply_saturating(
            fraction_days, HEBREW_DAY_PARTS));
    gint weekday = (gint)calendar_plus_positive_modulo(day, 7);

    if (weekday == 2 || weekday == 4 || weekday == 6 ||
        (weekday == 0 &&
         fraction >= 21 * HEBREW_HOUR_PARTS + 589 &&
         hebrew_is_leap(
             calendar_plus_i64_subtract_saturating(year, 1))))
    {
        day = calendar_plus_i64_add_saturating(day, 1);
    }
    else if (weekday == 1 &&
             fraction >= 15 * HEBREW_HOUR_PARTS + 204 &&
             !hebrew_is_leap(year))
    {
        day = calendar_plus_i64_add_saturating(day, 2);
    }

    return day;
}

static gint
hebrew_year_type(gint64 year)
{
    gint64 length = calendar_plus_i64_subtract_saturating(
        hebrew_start_of_year(
            calendar_plus_i64_add_saturating(year, 1)),
        hebrew_start_of_year(year));

    if (length > 380)
        length = calendar_plus_i64_subtract_saturating(length, 30);

    if (length == 353)
        return 0;
    if (length == 355)
        return 2;
    return 1;
}

static gint
hebrew_month_length(gint64 year,
                    gint month)
{
    const gint month_index = month - 1;

    if (month < 1 || month > 13)
        return 0;
    if (month_index == 5 && !hebrew_is_leap(year))
        return 0;
    if (month_index == 1 || month_index == 2)
        return hebrew_month_length_table[month_index][
            hebrew_year_type(year)];
    return hebrew_month_length_table[month_index][0];
}

static gint
hebrew_month_start_offset(gint64 year,
                          gint month)
{
    const gint month_index = month - 1;
    const gint type = hebrew_year_type(year);

    if (month < 1 || month > 13)
        return 0;
    return hebrew_is_leap(year) ?
        hebrew_leap_month_start[month_index][type] :
        hebrew_month_start[month_index][type];
}

static gint64
hebrew_to_jdn(gint64 year,
              gint month,
              gint day)
{
    gint64 result = calendar_plus_i64_add_saturating(
        hebrew_start_of_year(year), HEBREW_EPOCH_OFFSET);

    result = calendar_plus_i64_add_saturating(
        result, hebrew_month_start_offset(year, month));
    return calendar_plus_i64_add_saturating(result, day - 1);
}

static void
hebrew_from_jdn(gint64 jdn,
                CalendarPlusCalendarFields *fields)
{
    const gint64 d = calendar_plus_i64_subtract_saturating(
        jdn, HEBREW_EPOCH_OFFSET - 1);
    const gint64 approximate_months = calendar_plus_floor_divide(
        calendar_plus_i64_multiply_saturating(
            d, HEBREW_DAY_PARTS),
        HEBREW_MONTH_PARTS);
    gint64 year = calendar_plus_i64_add_saturating(
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(
                    19, approximate_months),
                234),
            235),
        1);
    gint64 day_of_year = calendar_plus_i64_subtract_saturating(
        d, hebrew_start_of_year(year));
    gint month_index = 0;
    gint type;
    gboolean leap;

    while (day_of_year < 1)
    {
        year = calendar_plus_i64_subtract_saturating(year, 1);
        day_of_year = calendar_plus_i64_subtract_saturating(
            d, hebrew_start_of_year(year));
    }

    type = hebrew_year_type(year);
    leap = hebrew_is_leap(year);
    while (month_index < 14 &&
           day_of_year >
               (leap ?
                    hebrew_leap_month_start[month_index][type] :
                    hebrew_month_start[month_index][type]))
    {
        month_index++;
    }
    if (month_index > 0)
        month_index--;

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month_index + 1,
        .day = (gint)calendar_plus_i64_subtract_saturating(
            day_of_year,
            leap ?
                hebrew_leap_month_start[month_index][type] :
                hebrew_month_start[month_index][type]),
        .auxiliary = 0,
        .special = FALSE
    };
}

static void
hebrew_add_months_to_fields(CalendarPlusCalendarFields *fields,
                            gint amount)
{
    gint64 year = fields->year;
    gint month = fields->month - 1;
    gint remaining = amount;

    if (remaining >= 235 || remaining <= -235)
    {
        const gint cycles = remaining / 235;

        year = calendar_plus_i64_add_saturating(
            year, calendar_plus_i64_multiply_saturating(cycles, 19));
        remaining -= cycles * 235;
    }

    while (remaining > 0)
    {
        month++;
        if (month > 12)
        {
            month = 0;
            year = calendar_plus_i64_add_saturating(year, 1);
        }
        if (month == 5 && !hebrew_is_leap(year))
            month++;
        remaining--;
    }

    while (remaining < 0)
    {
        month--;
        if (month < 0)
        {
            year = calendar_plus_i64_subtract_saturating(year, 1);
            month = 12;
        }
        if (month == 5 && !hebrew_is_leap(year))
            month--;
        remaining++;
    }

    fields->year = year;
    fields->month = month + 1;
    fields->day = MIN(
        fields->day, hebrew_month_length(year, fields->month));
}

static const gint persian_non_leap_corrections[] = {
    1502, 1601, 1634, 1667, 1700, 1733, 1766, 1799,
    1832, 1865, 1898, 1931, 1964, 1997, 2030, 2059,
    2063, 2096, 2129, 2158, 2162, 2191, 2195, 2224,
    2228, 2257, 2261, 2290, 2294, 2323, 2327, 2356,
    2360, 2389, 2393, 2422, 2426, 2455, 2459, 2488,
    2492, 2521, 2525, 2554, 2558, 2587, 2591, 2620,
    2624, 2653, 2657, 2686, 2690, 2719, 2723, 2748,
    2752, 2756, 2781, 2785, 2789, 2818, 2822, 2847,
    2851, 2855, 2880, 2884, 2888, 2913, 2917, 2921,
    2946, 2950, 2954, 2979, 2983, 2987
};

static gboolean
persian_is_correction_year(gint64 year)
{
    gsize index;

    if (year < persian_non_leap_corrections[0] ||
        year > persian_non_leap_corrections[
            G_N_ELEMENTS(persian_non_leap_corrections) - 1])
    {
        return FALSE;
    }

    for (index = 0;
         index < G_N_ELEMENTS(persian_non_leap_corrections);
         index++)
    {
        if (year == persian_non_leap_corrections[index])
            return TRUE;
        if (year < persian_non_leap_corrections[index])
            return FALSE;
    }

    return FALSE;
}

static gboolean
persian_is_leap(gint64 year)
{
    if (persian_is_correction_year(year))
        return FALSE;
    if (persian_is_correction_year(
            calendar_plus_i64_subtract_saturating(year, 1)))
    {
        return TRUE;
    }

    return calendar_plus_positive_modulo(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(year, 25), 11),
        33) < 8;
}

static gint64
persian_first_day_offset(gint64 year)
{
    gint64 result = calendar_plus_i64_multiply_saturating(
        365, calendar_plus_i64_subtract_saturating(year, 1));

    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(8, year), 21),
            33));
    if (year > persian_non_leap_corrections[0] &&
        persian_is_correction_year(
            calendar_plus_i64_subtract_saturating(year, 1)))
    {
        result = calendar_plus_i64_subtract_saturating(result, 1);
    }
    return result;
}

static gint
persian_month_length(gint64 year,
                     gint month)
{
    if (month < 1 || month > 12)
        return 0;
    if (month <= 6)
        return 31;
    if (month <= 11)
        return 30;
    return persian_is_leap(year) ? 30 : 29;
}

static gint64
persian_to_jdn(gint64 year,
               gint month,
               gint day)
{
    static const gint month_offsets[] = {
        0, 0, 31, 62, 93, 124, 155, 186,
        216, 246, 276, 306, 336
    };
    gint64 result;

    if (month < 1 || month > 12)
        return CALENDAR_PLUS_PERSIAN_EPOCH_JDN;

    result = calendar_plus_i64_add_saturating(
        CALENDAR_PLUS_PERSIAN_EPOCH_JDN,
        persian_first_day_offset(year));
    result = calendar_plus_i64_add_saturating(
        result, month_offsets[month]);
    return calendar_plus_i64_add_saturating(result, day - 1);
}

static void
persian_from_jdn(gint64 jdn,
                 CalendarPlusCalendarFields *fields)
{
    static const gint month_offsets[] = {
        0, 0, 31, 62, 93, 124, 155, 186,
        216, 246, 276, 306, 336
    };
    const gint64 days_since_epoch = calendar_plus_i64_subtract_saturating(
        jdn, CALENDAR_PLUS_PERSIAN_EPOCH_JDN);
    gint64 year = calendar_plus_i64_add_saturating(
        calendar_plus_floor_divide(
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(
                    33, days_since_epoch),
                3),
            12053),
        1);
    gint64 day_index = calendar_plus_i64_subtract_saturating(
        days_since_epoch, persian_first_day_offset(year));
    gint month;

    if (day_index == 365 && persian_is_correction_year(year))
    {
        year = calendar_plus_i64_add_saturating(year, 1);
        day_index = 0;
    }

    month = day_index < 216 ?
        (gint)calendar_plus_floor_divide(day_index, 31) + 1 :
        (gint)calendar_plus_floor_divide(
            calendar_plus_i64_subtract_saturating(day_index, 6), 30) + 1;

    *fields = (CalendarPlusCalendarFields){
        .year = year,
        .month = month,
        .day = (gint)calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                day_index, month_offsets[month]),
            1),
        .auxiliary = 0,
        .special = FALSE
    };
}

static gboolean
coptic_style_is_leap(gint64 year)
{
    return calendar_plus_positive_modulo(year, 4) == 3;
}

static gint
coptic_style_month_length(gint64 year,
                          gint month)
{
    if (month < 1 || month > 13)
        return 0;
    if (month <= 12)
        return 30;
    return coptic_style_is_leap(year) ? 6 : 5;
}

static gint64
coptic_style_to_jdn(gint64 year,
                    gint month,
                    gint day,
                    gint64 epoch)
{
    gint64 result = calendar_plus_i64_subtract_saturating(epoch, 1);

    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(
            365, calendar_plus_i64_subtract_saturating(year, 1)));
    result = calendar_plus_i64_add_saturating(
        result, calendar_plus_floor_divide(year, 4));
    result = calendar_plus_i64_add_saturating(
        result,
        calendar_plus_i64_multiply_saturating(30, month - 1));
    return calendar_plus_i64_add_saturating(result, day);
}

static gint64
coptic_style_year_from_jdn(gint64 jdn,
                           gint64 epoch)
{
    return calendar_plus_floor_divide(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(
                4, calendar_plus_i64_subtract_saturating(jdn, epoch)),
            1463),
        1461);
}

static void
coptic_style_from_jdn(CalendarPlusCalendarMode mode,
                      gint64 jdn,
                      gint64 epoch,
                      CalendarPlusCalendarFields *fields)
{
    const gint64 year = coptic_style_year_from_jdn(jdn, epoch);
    const gint64 start = coptic_style_to_jdn(year, 1, 1, epoch);
    const gint month = (gint)calendar_plus_i64_add_saturating(
        calendar_plus_floor_divide(
            calendar_plus_i64_subtract_saturating(jdn, start), 30),
        1);

    *fields = (CalendarPlusCalendarFields){
        .month = month,
        .day = (gint)calendar_plus_i64_add_saturating(
            calendar_plus_i64_subtract_saturating(
                jdn, coptic_style_to_jdn(year, month, 1, epoch)),
            1),
        .special = FALSE
    };
    set_signed_year(mode, year, fields);
}

static gint64
indian_year_start_jdn(gint64 gregorian_year)
{
    return calendar_plus_gregorian_to_jdn(
        gregorian_year,
        3,
        calendar_plus_gregorian_is_leap(gregorian_year) ? 21 : 22);
}

static gint
indian_month_length_for_year(gint64 saka_year,
                             gint month)
{
    const gint64 gregorian_year = calendar_plus_i64_add_saturating(
        saka_year, INDIAN_GREGORIAN_OFFSET);

    if (month < 1 || month > 12)
        return 0;
    if (month == 1)
        return calendar_plus_gregorian_is_leap(gregorian_year) ? 31 : 30;
    if (month <= 6)
        return 31;
    return 30;
}

static gint64
indian_to_jdn(gint64 saka_year,
              gint month,
              gint day)
{
    const gint64 gregorian_year = calendar_plus_i64_add_saturating(
        saka_year, INDIAN_GREGORIAN_OFFSET);
    const gint chaitra =
        calendar_plus_gregorian_is_leap(gregorian_year) ? 31 : 30;
    gint64 offset;

    if (month <= 1)
    {
        offset = day - 1;
    }
    else if (month <= 6)
    {
        offset = calendar_plus_i64_add_saturating(
            chaitra,
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(31, month - 2),
                day - 1));
    }
    else
    {
        offset = calendar_plus_i64_add_saturating(
            chaitra + 5 * 31,
            calendar_plus_i64_add_saturating(
                calendar_plus_i64_multiply_saturating(30, month - 7),
                day - 1));
    }

    return calendar_plus_i64_add_saturating(
        indian_year_start_jdn(gregorian_year), offset);
}

static void
indian_from_jdn(gint64 jdn,
                CalendarPlusCalendarFields *fields)
{
    gint gregorian_year;
    gint gregorian_month;
    gint gregorian_day;
    gint64 start_year;
    gint64 start;
    gint64 saka_year;
    gint64 day_index;
    gint month;
    gint day;
    gint chaitra;

    calendar_plus_jdn_to_gregorian(
        jdn, &gregorian_year, &gregorian_month, &gregorian_day);
    (void)gregorian_month;
    (void)gregorian_day;

    start_year = gregorian_year;
    start = indian_year_start_jdn(start_year);
    if (jdn < start)
    {
        start_year = calendar_plus_i64_subtract_saturating(start_year, 1);
        start = indian_year_start_jdn(start_year);
    }

    saka_year = calendar_plus_i64_subtract_saturating(
        start_year, INDIAN_GREGORIAN_OFFSET);
    day_index = calendar_plus_i64_subtract_saturating(jdn, start);
    chaitra = calendar_plus_gregorian_is_leap(start_year) ? 31 : 30;

    if (day_index < chaitra)
    {
        month = 1;
        day = (gint)day_index + 1;
    }
    else if (day_index < chaitra + 5 * 31)
    {
        const gint64 remaining =
            calendar_plus_i64_subtract_saturating(day_index, chaitra);

        month = (gint)calendar_plus_floor_divide(remaining, 31) + 2;
        day = (gint)calendar_plus_positive_modulo(remaining, 31) + 1;
    }
    else
    {
        const gint64 remaining = calendar_plus_i64_subtract_saturating(
            day_index, chaitra + 5 * 31);

        month = (gint)calendar_plus_floor_divide(remaining, 30) + 7;
        day = (gint)calendar_plus_positive_modulo(remaining, 30) + 1;
    }

    *fields = (CalendarPlusCalendarFields){
        .year = saka_year,
        .month = month,
        .day = day,
        .auxiliary = 0,
        .special = FALSE
    };
}

static gint
periods_per_year(CalendarPlusCalendarMode mode)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return 13;
        default:
            return 12;
    }
}

static gint
month_length(CalendarPlusCalendarMode mode,
             const CalendarPlusCalendarFields *fields)
{
    const gint64 year = signed_year_from_fields(mode, fields);

    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            return calendar_plus_gregorian_month_length(
                year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return islamic_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return umm_al_qura_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return coptic_style_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            return indian_month_length_for_year(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            return persian_month_length(year, fields->month);

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            return hebrew_month_length(year, fields->month);

        default:
            return 0;
    }
}

static gint64
fields_to_jdn(CalendarPlusCalendarMode mode,
              const CalendarPlusCalendarFields *fields)
{
    const gint64 year = signed_year_from_fields(mode, fields);

    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            return calendar_plus_gregorian_to_jdn(
                year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
            return islamic_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return islamic_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return umm_al_qura_to_jdn(
                year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            return coptic_style_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_COPTIC_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            return coptic_style_to_jdn(
                year, fields->month, fields->day,
                CALENDAR_PLUS_ETHIOPIC_EPOCH_JDN);

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            return indian_to_jdn(year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            return persian_to_jdn(year, fields->month, fields->day);

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            return hebrew_to_jdn(year, fields->month, fields->day);

        default:
            return CALENDAR_PLUS_UNIX_EPOCH_JDN;
    }
}

gboolean
calendar_plus_arithmetic_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    gint year;
    gint month;
    gint day;

    g_return_val_if_fail(fields != NULL, FALSE);
    *fields = (CalendarPlusCalendarFields){ 0 };

    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_GREGORIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_BUDDHIST:
        case CALENDAR_PLUS_CALENDAR_MODE_MINGUO:
            calendar_plus_jdn_to_gregorian(jdn, &year, &month, &day);
            fields->month = month;
            fields->day = day;
            fields->special = FALSE;
            set_signed_year(mode, year, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_JAPANESE:
            japanese_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
            islamic_from_jdn(
                jdn, CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            islamic_from_jdn(
                jdn, CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return umm_al_qura_from_jdn(jdn, fields);

        case CALENDAR_PLUS_CALENDAR_MODE_COPTIC:
            coptic_style_from_jdn(
                mode, jdn, CALENDAR_PLUS_COPTIC_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIAN:
        case CALENDAR_PLUS_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM:
            coptic_style_from_jdn(
                mode, jdn, CALENDAR_PLUS_ETHIOPIC_EPOCH_JDN, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_INDIAN:
            indian_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_PERSIAN:
            persian_from_jdn(jdn, fields);
            return TRUE;

        case CALENDAR_PLUS_CALENDAR_MODE_HEBREW:
            hebrew_from_jdn(jdn, fields);
            return TRUE;

        default:
            return FALSE;
    }
}

gint64
calendar_plus_arithmetic_month_start(
    CalendarPlusCalendarMode mode,
    gint64 jdn)
{
    CalendarPlusCalendarFields fields;

    if (!arithmetic_mode_supported(mode) ||
        !calendar_plus_arithmetic_fields_from_jdn(mode, jdn, &fields))
    {
        return jdn;
    }

    fields.day = 1;
    return fields_to_jdn(mode, &fields);
}

gint64
calendar_plus_arithmetic_add_months(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount)
{
    CalendarPlusCalendarFields fields;
    gint64 year;
    gint64 serial;
    gint period_count;
    gint target_month;

    if (!arithmetic_mode_supported(mode) ||
        !calendar_plus_arithmetic_fields_from_jdn(mode, jdn, &fields))
    {
        return jdn;
    }

    if (mode == CALENDAR_PLUS_CALENDAR_MODE_HEBREW)
    {
        hebrew_add_months_to_fields(&fields, amount);
        return fields_to_jdn(mode, &fields);
    }

    year = signed_year_from_fields(mode, &fields);
    period_count = periods_per_year(mode);
    serial = calendar_plus_i64_add_saturating(
        calendar_plus_i64_add_saturating(
            calendar_plus_i64_multiply_saturating(year, period_count),
            fields.month - 1),
        amount);
    year = calendar_plus_floor_divide(serial, period_count);
    target_month =
        (gint)calendar_plus_positive_modulo(serial, period_count) + 1;

    set_signed_year(mode, year, &fields);
    if (mode == CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA &&
        (year < UMM_AL_QURA_FIRST_YEAR || year > UMM_AL_QURA_LAST_YEAR))
    {
        return jdn;
    }
    fields.month = target_month;
    fields.day = MIN(fields.day, month_length(mode, &fields));
    return fields_to_jdn(mode, &fields);
}

gint64
calendar_plus_arithmetic_add_years(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    gint amount)
{
    CalendarPlusCalendarFields fields;
    gint64 year;

    if (!arithmetic_mode_supported(mode) ||
        !calendar_plus_arithmetic_fields_from_jdn(mode, jdn, &fields))
    {
        return jdn;
    }

    if (mode == CALENDAR_PLUS_CALENDAR_MODE_HEBREW)
    {
        fields.year = calendar_plus_i64_add_saturating(
            fields.year, amount);
        if (fields.month == 6 && !hebrew_is_leap(fields.year))
            fields.month = 7;
        fields.day = MIN(
            fields.day, hebrew_month_length(fields.year, fields.month));
        return fields_to_jdn(mode, &fields);
    }

    year = calendar_plus_i64_add_saturating(
        signed_year_from_fields(mode, &fields), amount);
    if (mode == CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA &&
        (year < UMM_AL_QURA_FIRST_YEAR || year > UMM_AL_QURA_LAST_YEAR))
    {
        return jdn;
    }
    set_signed_year(mode, year, &fields);
    fields.day = MIN(fields.day, month_length(mode, &fields));
    return fields_to_jdn(mode, &fields);
}
