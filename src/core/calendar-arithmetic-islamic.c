// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Calendar-owned Islamic arithmetic families.
 *
 * Civil/tabular arithmetic and the bounded Umm al-Qura table stay in one
 * module so their epochs, month rules and conversion boundaries are reviewed
 * together.  No locale presentation lives here.
 */

#include "calendar-arithmetic-internal.h"

#include "integer-math.h"
#include "julian-day.h"

enum
{
    CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN = 1948440,
    CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN = 1948439
};

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

gboolean
calendar_plus_arithmetic_islamic_fields_from_jdn(
    CalendarPlusCalendarMode mode,
    gint64 jdn,
    CalendarPlusCalendarFields *fields)
{
    switch (mode)
    {
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
        default:
            return FALSE;
    }
}

gint64
calendar_plus_arithmetic_islamic_fields_to_jdn(
    CalendarPlusCalendarMode mode,
    gint64 year,
    gint month,
    gint day)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
            return islamic_to_jdn(
                year, month, day, CALENDAR_PLUS_ISLAMIC_CIVIL_EPOCH_JDN);
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return islamic_to_jdn(
                year, month, day, CALENDAR_PLUS_ISLAMIC_TBLA_EPOCH_JDN);
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return umm_al_qura_to_jdn(year, month, day);
        default:
            return CALENDAR_PLUS_UNIX_EPOCH_JDN;
    }
}

gint
calendar_plus_arithmetic_islamic_month_length(
    CalendarPlusCalendarMode mode,
    gint64 year,
    gint month)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return islamic_month_length(year, month);
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return umm_al_qura_month_length(year, month);
        default:
            return 0;
    }
}

gboolean
calendar_plus_arithmetic_islamic_year_supported(
    CalendarPlusCalendarMode mode,
    gint64 year)
{
    switch (mode)
    {
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_CIVIL:
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_TBLA:
            return TRUE;
        case CALENDAR_PLUS_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA:
            return year >= UMM_AL_QURA_FIRST_YEAR &&
                   year <= UMM_AL_QURA_LAST_YEAR;
        default:
            return FALSE;
    }
}
