// SPDX-License-Identifier: GPL-3.0-or-later
/* Resource-owned descriptors; stable IDs are never array positions. */
#if defined(RESOURCE_ISO_WEEK)
#define RESOURCE_ID "iso-week"
#define RESOURCE_NAME "ISO week calendar"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ISO_WEEK
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_JULIAN)
#define RESOURCE_ID "julian"
#define RESOURCE_NAME "Julian"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_JULIAN
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_REVISED_JULIAN)
#define RESOURCE_ID "revised-julian"
#define RESOURCE_NAME "Revised Julian"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_REVISED_JULIAN
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_ISLAMIC_UMM_AL_QURA)
#define RESOURCE_ID "islamic-umalqura"
#define RESOURCE_NAME "Islamic (Umm al-Qura table; AH 1300–1600)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ISLAMIC_UMM_AL_QURA
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "islamic-umalqura"
#elif defined(RESOURCE_ISLAMIC_CIVIL)
#define RESOURCE_ID "islamic-civil"
#define RESOURCE_NAME "Islamic (civil/tabular)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ISLAMIC_CIVIL
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "islamic-civil"
#elif defined(RESOURCE_ISLAMIC_TBLA)
#define RESOURCE_ID "islamic-tbla"
#define RESOURCE_NAME "Islamic (tabular, astronomical epoch)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ISLAMIC_TBLA
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "islamic-tbla"
#elif defined(RESOURCE_ISLAMIC)
#define RESOURCE_ID "islamic"
#define RESOURCE_NAME "Islamic (computational/astronomical; not local sighting)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ISLAMIC
#define RESOURCE_FAMILY_ICU 1
#define RESOURCE_KEYWORD "islamic"
#elif defined(RESOURCE_BAHAI)
#define RESOURCE_ID "bahai"
#define RESOURCE_NAME "Bahá’í (Badíʿ; modern Naw-Rúz from Tehran equinox)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_BAHAI
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_BUDDHIST)
#define RESOURCE_ID "buddhist"
#define RESOURCE_NAME "Buddhist Era (Gregorian-derived; +543 years)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_BUDDHIST
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "buddhist"
#elif defined(RESOURCE_ETHIOPIAN)
#define RESOURCE_ID "ethiopian"
#define RESOURCE_NAME "Ethiopian Amete Mihret (13-month calendar)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ETHIOPIAN
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "ethiopic"
#elif defined(RESOURCE_ETHIOPIC_AMETE_ALEM)
#define RESOURCE_ID "ethiopic-amete-alem"
#define RESOURCE_NAME "Ethiopic Amete Alem (Amete Mihret + 5500 years)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ETHIOPIC_AMETE_ALEM
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "ethiopic-amete-alem"
#elif defined(RESOURCE_CHINESE)
#define RESOURCE_ID "chinese"
#define RESOURCE_NAME "Chinese traditional (computational lunisolar)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_CHINESE
#define RESOURCE_FAMILY_ICU 1
#define RESOURCE_KEYWORD "chinese"
#elif defined(RESOURCE_DANGI)
#define RESOURCE_ID "dangi"
#define RESOURCE_NAME "Dangi (traditional Korean computational lunisolar)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_DANGI
#define RESOURCE_FAMILY_ICU 1
#define RESOURCE_KEYWORD "dangi"
#elif defined(RESOURCE_INDIAN)
#define RESOURCE_ID "indian"
#define RESOURCE_NAME "Indian National (Saka)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_INDIAN
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "indian"
#elif defined(RESOURCE_JAPANESE)
#define RESOURCE_ID "japanese"
#define RESOURCE_NAME "Japanese imperial era (modern Meiji–Reiwa)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_JAPANESE
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "japanese"
#elif defined(RESOURCE_MINGUO)
#define RESOURCE_ID "minguo"
#define RESOURCE_NAME "Minguo / Republic of China (Year 1 = 1912)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_MINGUO
#define RESOURCE_FAMILY_ARITHMETIC 1
#define RESOURCE_KEYWORD "roc"
#elif defined(RESOURCE_ROMAN)
#define RESOURCE_ID "roman"
#define RESOURCE_NAME "Roman civil date notation (Julian; inclusive Kalends/Nones/Ides; Varronian A.U.C.)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ROMAN
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_BYZANTINE)
#define RESOURCE_ID "byzantine"
#define RESOURCE_NAME "Byzantine Anno Mundi — Constantinople (Julian; year begins 1 September)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_BYZANTINE
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_EGYPTIAN_NABONASSAR)
#define RESOURCE_ID "egyptian-nabonassar"
#define RESOURCE_NAME "Ptolemaic Nabonassar-era Egyptian civil calendar (365-day wandering year; epoch 747 BC)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_EGYPTIAN_NABONASSAR
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_ARMENIAN_TRADITIONAL)
#define RESOURCE_ID "armenian-traditional"
#define RESOURCE_NAME "Traditional Armenian wandering calendar (365-day year; era begins 11 July 552 Julian)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_ARMENIAN_TRADITIONAL
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_MAYAN)
#define RESOURCE_ID "mayan"
#define RESOURCE_NAME "Maya Long Count (Goodman–Martínez–Thompson 584283 correlation)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_MAYAN
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_FRENCH_REPUBLICAN)
#define RESOURCE_ID "french-republican"
#define RESOURCE_NAME "French Republican (epoch 22 September 1792; Romme arithmetic continuation after historical use)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_FRENCH_REPUBLICAN
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_SWEDISH_HISTORICAL)
#define RESOURCE_ID "swedish-historical"
#define RESOURCE_NAME "Swedish historical calendar (1700–1753, incl. 30 Feb 1712)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_SWEDISH_HISTORICAL
#define RESOURCE_FAMILY_SWEDISH 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_INTERNATIONAL_FIXED)
#define RESOURCE_ID "international-fixed"
#define RESOURCE_NAME "International Fixed (13 × 28-day months + Year Day)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_INTERNATIONAL_FIXED
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_WORLD)
#define RESOURCE_ID "world"
#define RESOURCE_NAME "World Calendar (quarter-based; intercalary World/Leap days)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_WORLD
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#elif defined(RESOURCE_POSITIVIST)
#define RESOURCE_ID "positivist"
#define RESOURCE_NAME "Positivist calendar (Comte; 13 × 28-day months + festival days; Year 1 = 1789)"
#define RESOURCE_MODE CALENDAR_CALENDAR_MODE_POSITIVIST
#define RESOURCE_FAMILY_CUSTOM 1
#define RESOURCE_KEYWORD ""
#else
#error Unknown calendar resource variant
#endif
