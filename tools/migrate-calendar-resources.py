#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""One-shot migration of Gregorian, Persian and Coptic to system resources."""

from pathlib import Path
import re

COMMON_SHA = "3b86318e5c2277db4f2e5b3c1c5b35cd3bfe3b09"


def replace_once(path: str, old: str, new: str) -> None:
    target = Path(path)
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(
            f"{path}: expected exactly one match for {old[:100]!r}, got {count}"
        )
    target.write_text(text.replace(old, new), encoding="utf-8")


def patch_makefile() -> None:
    path = Path("Makefile")
    text = path.read_text(encoding="utf-8")
    old_pin = "INFILTRATR_COMMON_COMMIT := 3f0e49a0075aa16210af92e6e2477061f4eac893"
    new_pin = f"INFILTRATR_COMMON_COMMIT := {COMMON_SHA}"
    if text.count(old_pin) != 1:
        raise SystemExit("Makefile Common pin not found exactly once")
    text = text.replace(old_pin, new_pin)

    old_vars = """CALENDAR_RESOURCE_BUILD_DIR := $(BUILD_DIR)/calendar-resources
HEBREW_RESOURCE_SOURCE := src/resources/calendars/hebrew.c
HEBREW_RESOURCE_MAP := src/resources/calendars/hebrew.map
HEBREW_RESOURCE := $(CALENDAR_RESOURCE_BUILD_DIR)/hebrew.so
TEST_CALENDAR_RESOURCE_CPPFLAG := \\
\t-DCALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY=\\\"$(abspath $(CALENDAR_RESOURCE_BUILD_DIR))\\\"
RUNTIME_CALENDAR_FIXTURE_DIR := $(BUILD_DIR)/calendar-resource-fixture
RUNTIME_CALENDAR_FIXTURE := $(RUNTIME_CALENDAR_FIXTURE_DIR)/fixture-calendar.so
RUNTIME_HEBREW_RESOURCE := $(RUNTIME_CALENDAR_FIXTURE_DIR)/hebrew.so
"""
    new_vars = """CALENDAR_RESOURCE_BUILD_DIR := $(BUILD_DIR)/calendar-resources
CALENDAR_RESOURCE_MAP := src/resources/calendars/calendar-resource.map
CALENDAR_RESOURCE_NAMES := hebrew gregorian persian coptic
HEBREW_RESOURCE_SOURCE := src/resources/calendars/hebrew.c
GREGORIAN_RESOURCE_SOURCE := src/resources/calendars/gregorian.c
PERSIAN_RESOURCE_SOURCE := src/resources/calendars/persian.c
COPTIC_RESOURCE_SOURCE := src/resources/calendars/coptic.c
CALENDAR_RESOURCE_SOURCES := $(HEBREW_RESOURCE_SOURCE) \\
\t$(GREGORIAN_RESOURCE_SOURCE) \\
\t$(PERSIAN_RESOURCE_SOURCE) \\
\t$(COPTIC_RESOURCE_SOURCE)
HEBREW_RESOURCE := $(CALENDAR_RESOURCE_BUILD_DIR)/hebrew.so
GREGORIAN_RESOURCE := $(CALENDAR_RESOURCE_BUILD_DIR)/gregorian.so
PERSIAN_RESOURCE := $(CALENDAR_RESOURCE_BUILD_DIR)/persian.so
COPTIC_RESOURCE := $(CALENDAR_RESOURCE_BUILD_DIR)/coptic.so
CALENDAR_RESOURCES := $(HEBREW_RESOURCE) $(GREGORIAN_RESOURCE) \\
\t$(PERSIAN_RESOURCE) $(COPTIC_RESOURCE)
TEST_CALENDAR_RESOURCE_CPPFLAG := \\
\t-DCALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY=\\\"$(abspath $(CALENDAR_RESOURCE_BUILD_DIR))\\\"
RUNTIME_CALENDAR_FIXTURE_DIR := $(BUILD_DIR)/calendar-resource-fixture
RUNTIME_CALENDAR_FIXTURE := $(RUNTIME_CALENDAR_FIXTURE_DIR)/fixture-calendar.so
RUNTIME_CALENDAR_RESOURCES := $(addprefix $(RUNTIME_CALENDAR_FIXTURE_DIR)/,$(addsuffix .so,$(CALENDAR_RESOURCE_NAMES)))
"""
    if text.count(old_vars) != 1:
        raise SystemExit("Makefile resource variable block not found exactly once")
    text = text.replace(old_vars, new_vars)
    text = text.replace("$(HEBREW_RESOURCE_MAP)", "$(CALENDAR_RESOURCE_MAP)")

    dependency = "\t$(HEBREW_RESOURCE) \\\n"
    if text.count(dependency) != 5:
        raise SystemExit(
            f"Makefile expected five Hebrew-only resource dependencies, got {text.count(dependency)}"
        )
    text = text.replace(dependency, "\t$(CALENDAR_RESOURCES) \\\n")

    rule_tail = """\t\t-Wl,--version-script=$(CALENDAR_RESOURCE_MAP) \\
\t\t-o $@ $(LDFLAGS) $(CORE_LIBS) $(ICU_BRIDGE_LIBS) $(MATH_LIBS)
"""
    resource_rules = rule_tail + """
$(GREGORIAN_RESOURCE): $(GREGORIAN_RESOURCE_SOURCE) $(CALENDAR_RESOURCE_MAP) \\
\t\tsrc/core/julian-day.c src/core/icu-calendar.c \\
\t\tsrc/core/icu-compat-bridge.c $(INFILTRATR_COMMON_HEADERS) \\
\t\t$(INFILTRATR_COMMON_ARCHIVE)
\t@mkdir -p "$(CALENDAR_RESOURCE_BUILD_DIR)"
\t$(CC) $(CPPFLAGS) $(CFLAGS) -fvisibility=hidden \\
\t\t-frandom-seed=$(REPRO_SEED_PREFIX)-gregorian-resource $(CORE_CFLAGS) \\
\t\t-shared $(GREGORIAN_RESOURCE_SOURCE) src/core/julian-day.c \\
\t\tsrc/core/icu-calendar.c src/core/icu-compat-bridge.c \\
\t\t$(INFILTRATR_COMMON_ARCHIVE) \\
\t\t-Wl,--version-script=$(CALENDAR_RESOURCE_MAP) \\
\t\t-o $@ $(LDFLAGS) $(CORE_LIBS) $(ICU_BRIDGE_LIBS) $(MATH_LIBS)

$(PERSIAN_RESOURCE): $(PERSIAN_RESOURCE_SOURCE) $(CALENDAR_RESOURCE_MAP) \\
\t\tsrc/core/calendar-arithmetic-persian.c src/core/icu-calendar.c \\
\t\tsrc/core/icu-compat-bridge.c $(INFILTRATR_COMMON_HEADERS) \\
\t\t$(INFILTRATR_COMMON_ARCHIVE)
\t@mkdir -p "$(CALENDAR_RESOURCE_BUILD_DIR)"
\t$(CC) $(CPPFLAGS) $(CFLAGS) -fvisibility=hidden \\
\t\t-frandom-seed=$(REPRO_SEED_PREFIX)-persian-resource $(CORE_CFLAGS) \\
\t\t-shared $(PERSIAN_RESOURCE_SOURCE) src/core/calendar-arithmetic-persian.c \\
\t\tsrc/core/icu-calendar.c src/core/icu-compat-bridge.c \\
\t\t$(INFILTRATR_COMMON_ARCHIVE) \\
\t\t-Wl,--version-script=$(CALENDAR_RESOURCE_MAP) \\
\t\t-o $@ $(LDFLAGS) $(CORE_LIBS) $(ICU_BRIDGE_LIBS) $(MATH_LIBS)

$(COPTIC_RESOURCE): $(COPTIC_RESOURCE_SOURCE) $(CALENDAR_RESOURCE_MAP) \\
\t\tsrc/core/calendar-arithmetic-coptic.c src/core/icu-calendar.c \\
\t\tsrc/core/icu-compat-bridge.c $(INFILTRATR_COMMON_HEADERS) \\
\t\t$(INFILTRATR_COMMON_ARCHIVE)
\t@mkdir -p "$(CALENDAR_RESOURCE_BUILD_DIR)"
\t$(CC) $(CPPFLAGS) $(CFLAGS) -fvisibility=hidden \\
\t\t-frandom-seed=$(REPRO_SEED_PREFIX)-coptic-resource $(CORE_CFLAGS) \\
\t\t-shared $(COPTIC_RESOURCE_SOURCE) src/core/calendar-arithmetic-coptic.c \\
\t\tsrc/core/icu-calendar.c src/core/icu-compat-bridge.c \\
\t\t$(INFILTRATR_COMMON_ARCHIVE) \\
\t\t-Wl,--version-script=$(CALENDAR_RESOURCE_MAP) \\
\t\t-o $@ $(LDFLAGS) $(CORE_LIBS) $(ICU_BRIDGE_LIBS) $(MATH_LIBS)
"""
    if text.count(rule_tail) != 1:
        raise SystemExit("Hebrew resource build rule tail not found exactly once")
    text = text.replace(rule_tail, resource_rules)

    old_copy = """$(RUNTIME_HEBREW_RESOURCE): $(HEBREW_RESOURCE)
\t@mkdir -p "$(RUNTIME_CALENDAR_FIXTURE_DIR)"
\tcp "$(HEBREW_RESOURCE)" "$@"
"""
    new_copy = """$(RUNTIME_CALENDAR_FIXTURE_DIR)/%.so: $(CALENDAR_RESOURCE_BUILD_DIR)/%.so
\t@mkdir -p "$(RUNTIME_CALENDAR_FIXTURE_DIR)"
\tcp "$<" "$@"
"""
    if text.count(old_copy) != 1:
        raise SystemExit("runtime Hebrew copy rule not found exactly once")
    text = text.replace(old_copy, new_copy)
    text = text.replace("$(RUNTIME_HEBREW_RESOURCE)", "$(RUNTIME_CALENDAR_RESOURCES)")

    text = text.replace(
        "validate-exports: $(BUILD_DIR)/$(LIB_REALNAME) $(HEBREW_RESOURCE)",
        "validate-exports: $(BUILD_DIR)/$(LIB_REALNAME) $(CALENDAR_RESOURCES)",
    )
    old_exports = """\t@nm -D --defined-only --format=posix "$(HEBREW_RESOURCE)" | \\
\t\tawk '{ sub(/@.*/, "", $$1); print $$1 }' | LC_ALL=C sort -u > \\
\t\t"$(BUILD_DIR)/hebrew-resource-exports.actual"
\t@printf '%s\\n' infiltratr_temporal_calendar_provider_v1 infiltratr_temporal_resource_metadata_v1 | \\
\t\tLC_ALL=C sort -u > "$(BUILD_DIR)/hebrew-resource-exports.expected"
\tdiff -u "$(BUILD_DIR)/hebrew-resource-exports.expected" \\
\t\t"$(BUILD_DIR)/hebrew-resource-exports.actual"
"""
    new_exports = """\t@printf '%s\\n' infiltratr_temporal_calendar_provider_v1 infiltratr_temporal_resource_metadata_v1 | \\
\t\tLC_ALL=C sort -u > "$(BUILD_DIR)/calendar-resource-exports.expected"
\t@for resource in $(CALENDAR_RESOURCES); do \\
\t\tnm -D --defined-only --format=posix "$$resource" | \\
\t\t\tawk '{ sub(/@.*/, "", $$1); print $$1 }' | LC_ALL=C sort -u > \\
\t\t\t"$(BUILD_DIR)/calendar-resource-exports.actual"; \\
\t\tdiff -u "$(BUILD_DIR)/calendar-resource-exports.expected" \\
\t\t\t"$(BUILD_DIR)/calendar-resource-exports.actual" || exit 1; \\
\tdone
"""
    if text.count(old_exports) != 1:
        raise SystemExit("resource export validation block not found exactly once")
    text = text.replace(old_exports, new_exports)

    text = text.replace(
        "validate-runtime-deps: $(BUILD_DIR)/$(LIB_REALNAME) $(HEBREW_RESOURCE)",
        "validate-runtime-deps: $(BUILD_DIR)/$(LIB_REALNAME) $(CALENDAR_RESOURCES)",
    )
    text = text.replace(
        '@for binary in "$(BUILD_DIR)/$(LIB_REALNAME)" "$(HEBREW_RESOURCE)"; do',
        '@for binary in "$(BUILD_DIR)/$(LIB_REALNAME)" $(CALENDAR_RESOURCES); do',
    )
    text = text.replace(
        "$(SOURCES) $(HEBREW_RESOURCE_SOURCE) -- $(CPPFLAGS) $(GLIB_CFLAGS)",
        "$(SOURCES) $(CALENDAR_RESOURCE_SOURCES) -- $(CPPFLAGS) $(GLIB_CFLAGS)",
    )

    old_install = """\tinstall -Dm755 "$(HEBREW_RESOURCE)" \\
\t\t"$(DESTDIR)/System/Resources/Calendars/hebrew.so"
"""
    new_install = """\t@for resource in $(CALENDAR_RESOURCES); do \\
\t\tinstall -Dm755 "$$resource" \\
\t\t\t"$(DESTDIR)/System/Resources/Calendars/$$(basename "$$resource")"; \\
\tdone
"""
    if text.count(old_install) != 1:
        raise SystemExit("resource install block not found exactly once")
    text = text.replace(old_install, new_install)
    path.write_text(text, encoding="utf-8")


def patch_registry() -> None:
    path = Path("src/core/calendar-registry.c")
    text = path.read_text(encoding="utf-8")
    replacements = {
        '    ARITHMETIC_PROVIDER(GREGORIAN, "gregorian", "gregorian"),':
            '    [CALENDAR_CALENDAR_MODE_GREGORIAN] = { 0 },',
        '    ARITHMETIC_PROVIDER(PERSIAN, "persian", "persian"),':
            '    [CALENDAR_CALENDAR_MODE_PERSIAN] = { 0 },',
        '    ARITHMETIC_PROVIDER(COPTIC, "coptic", "coptic"),':
            '    [CALENDAR_CALENDAR_MODE_COPTIC] = { 0 },',
    }
    for old, new in replacements.items():
        if text.count(old) != 1:
            raise SystemExit(f"registry entry missing: {old}")
        text = text.replace(old, new)

    old_lookup = """        if (infiltratr_string_equal(
                calendar_id, providers[index].interface.calendar_id))
"""
    new_lookup = """        if (providers[index].interface.calendar_id != NULL &&
            infiltratr_string_equal(
                calendar_id, providers[index].interface.calendar_id))
"""
    if text.count(old_lookup) != 1:
        raise SystemExit("registry built-in lookup not found")
    text = text.replace(old_lookup, new_lookup)

    old_count_at = """gsize
calendar_calendar_provider_get_count(void)
{
    ensure_runtime_registry();
    return G_N_ELEMENTS(providers) +
        (runtime_providers != NULL ? (gsize)runtime_providers->len : 0U);
}

const CalendarCalendarProvider *
calendar_calendar_provider_at(gsize index)
{
    ensure_runtime_registry();
    if (index < G_N_ELEMENTS(providers))
        return &providers[index].interface;

    index -= G_N_ELEMENTS(providers);
    if (runtime_providers == NULL || index >= (gsize)runtime_providers->len)
        return NULL;

    return &((RuntimeCalendarProvider *)
        g_ptr_array_index(runtime_providers, (guint)index))->interface;
}
"""
    new_count_at = """static gsize
builtin_provider_count(void)
{
    gsize index;
    gsize count = 0U;

    for (index = 0; index < G_N_ELEMENTS(providers); index++)
    {
        if (providers[index].interface.calendar_id != NULL)
            count++;
    }
    return count;
}

gsize
calendar_calendar_provider_get_count(void)
{
    ensure_runtime_registry();
    return builtin_provider_count() +
        (runtime_providers != NULL ? (gsize)runtime_providers->len : 0U);
}

const CalendarCalendarProvider *
calendar_calendar_provider_at(gsize index)
{
    gsize builtin_index;

    ensure_runtime_registry();
    for (builtin_index = 0;
         builtin_index < G_N_ELEMENTS(providers);
         builtin_index++)
    {
        if (providers[builtin_index].interface.calendar_id == NULL)
            continue;
        if (index == 0U)
            return &providers[builtin_index].interface;
        index--;
    }

    if (runtime_providers == NULL || index >= (gsize)runtime_providers->len)
        return NULL;

    return &((RuntimeCalendarProvider *)
        g_ptr_array_index(runtime_providers, (guint)index))->interface;
}
"""
    if text.count(old_count_at) != 1:
        raise SystemExit("registry count/enumeration block not found")
    path.write_text(text.replace(old_count_at, new_count_at), encoding="utf-8")


def patch_debian_rules() -> None:
    path = Path("debian/rules")
    text = path.read_text(encoding="utf-8")
    old_stage = """\tinstall -d \\
\t\t"$(CURDIR)/debian/infiltrator-calendar-resources/System/Resources/Calendars"
\tmv "$(CURDIR)/debian/calendar/System/Resources/Calendars/hebrew.so" \\
\t\t"$(CURDIR)/debian/infiltrator-calendar-resources/System/Resources/Calendars/hebrew.so"
"""
    new_stage = """\tinstall -d \\
\t\t"$(CURDIR)/debian/infiltrator-calendar-resources/System/Resources/Calendars"
\tmv "$(CURDIR)/debian/calendar/System/Resources/Calendars/"*.so \\
\t\t"$(CURDIR)/debian/infiltrator-calendar-resources/System/Resources/Calendars/"
"""
    if text.count(old_stage) != 1:
        raise SystemExit("Debian resource staging block not found")
    text = text.replace(old_stage, new_stage)
    old_checks = """\t@test -f \\
\t\t"$(CURDIR)/debian/infiltrator-calendar-resources/System/Resources/Calendars/hebrew.so"
\t@test ! -e \\
\t\t"$(CURDIR)/debian/calendar/System/Resources/Calendars/hebrew.so"
"""
    new_checks = """\t@for resource in hebrew gregorian persian coptic; do \\
\t\ttest -f "$(CURDIR)/debian/infiltrator-calendar-resources/System/Resources/Calendars/$$resource.so"; \\
\t\ttest ! -e "$(CURDIR)/debian/calendar/System/Resources/Calendars/$$resource.so"; \\
\tdone
"""
    if text.count(old_checks) != 1:
        raise SystemExit("Debian resource ownership checks not found")
    path.write_text(text.replace(old_checks, new_checks), encoding="utf-8")


def patch_release_validator() -> None:
    path = Path("tools/validate-release-artifacts.sh")
    text = path.read_text(encoding="utf-8")
    old_package = """test ! -e "$TMP/generic/System/Resources/Calendars/hebrew.so"
test -f "$TMP/resources/System/Resources/Calendars/hebrew.so"
[ "$(find "$TMP/resources/System/Resources/Calendars" -maxdepth 1 -type f | wc -l)" -ge 1 ]
"""
    new_package = """for resource in hebrew gregorian persian coptic; do
    test ! -e "$TMP/generic/System/Resources/Calendars/$resource.so"
    test -f "$TMP/resources/System/Resources/Calendars/$resource.so"
done
[ "$(find "$TMP/resources/System/Resources/Calendars" -maxdepth 1 -type f -name '*.so' | wc -l)" -eq 4 ]
"""
    if text.count(old_package) != 1:
        raise SystemExit("release artifact resource ownership block not found")
    text = text.replace(old_package, new_package)

    old_icu = """if readelf -d "$TMP/resources/System/Resources/Calendars/hebrew.so" 2>/dev/null | \\
        grep -Eq 'Shared library: \\[libicu[^]]*\\.so\\.[0-9]+'; then
    echo "Calendar resource has a direct build-host ICU SONAME dependency" >&2
    exit 1
fi
"""
    new_icu = """for resource in hebrew gregorian persian coptic; do
    if readelf -d "$TMP/resources/System/Resources/Calendars/$resource.so" 2>/dev/null | \\
            grep -Eq 'Shared library: \\[libicu[^]]*\\.so\\.[0-9]+'; then
        echo "Calendar resource $resource has a direct build-host ICU SONAME dependency" >&2
        exit 1
    fi
done
"""
    if text.count(old_icu) != 1:
        raise SystemExit("release artifact ICU dependency block not found")
    text = text.replace(old_icu, new_icu)

    old_exports = """nm -D --defined-only --format=posix \\
    "$TMP/resources/System/Resources/Calendars/hebrew.so" | \\
    awk '{ sub(/@.*/, "", $1); print $1 }' | LC_ALL=C sort -u > \\
    "$TMP/resource-exports.actual"
printf '%s\\n' \\
    infiltratr_temporal_calendar_provider_v1 \\
    infiltratr_temporal_resource_metadata_v1 | LC_ALL=C sort -u > \\
    "$TMP/resource-exports.expected"
diff -u "$TMP/resource-exports.expected" "$TMP/resource-exports.actual"
"""
    new_exports = """printf '%s\\n' \\
    infiltratr_temporal_calendar_provider_v1 \\
    infiltratr_temporal_resource_metadata_v1 | LC_ALL=C sort -u > \\
    "$TMP/resource-exports.expected"
for resource in hebrew gregorian persian coptic; do
    nm -D --defined-only --format=posix \\
        "$TMP/resources/System/Resources/Calendars/$resource.so" | \\
        awk '{ sub(/@.*/, "", $1); print $1 }' | LC_ALL=C sort -u > \\
        "$TMP/resource-exports.actual"
    diff -u "$TMP/resource-exports.expected" "$TMP/resource-exports.actual"
done
"""
    if text.count(old_exports) != 1:
        raise SystemExit("release artifact resource export block not found")
    path.write_text(text.replace(old_exports, new_exports), encoding="utf-8")


def patch_ci_and_contracts() -> None:
    ci = Path(".github/workflows/ci.yml")
    text = ci.read_text(encoding="utf-8")
    pattern = re.compile(
        r'^(\s*)test -f /System/Resources/Calendars/hebrew\.so$', re.MULTILINE
    )
    matches = list(pattern.finditer(text))
    if not matches:
        raise SystemExit("CI Hebrew resource checks not found")
    text = pattern.sub(
        lambda match: (
            match.group(1) + "for resource in hebrew gregorian persian coptic; do\n"
            + match.group(1)
            + '  test -f "/System/Resources/Calendars/${resource}.so"\n'
            + match.group(1)
            + "done"
        ),
        text,
    )
    ci.write_text(text, encoding="utf-8")

    release_model = Path("tests/test-release-model.py")
    text = release_model.read_text(encoding="utf-8")
    old_rule_assert = (
        '    assert "debian/calendar/System/Resources/Calendars/hebrew.so" in rules\n'
    )
    new_rule_assert = (
        '    assert "debian/calendar/System/Resources/Calendars/\\\"*.so" in rules\n'
    )
    if text.count(old_rule_assert) != 1:
        raise SystemExit("release-model Debian resource assertion not found")
    text = text.replace(old_rule_assert, new_rule_assert)
    old_ci_assert = (
        '    assert "test -f /System/Resources/Calendars/hebrew.so" in package_install\n'
    )
    new_ci_assert = (
        '    assert "for resource in hebrew gregorian persian coptic" in package_install\n'
    )
    if text.count(old_ci_assert) != 1:
        raise SystemExit("release-model CI resource assertion not found")
    release_model.write_text(text.replace(old_ci_assert, new_ci_assert), encoding="utf-8")


def main() -> None:
    patch_makefile()
    patch_registry()
    patch_debian_rules()
    patch_release_validator()
    patch_ci_and_contracts()

    old_map = Path("src/resources/calendars/hebrew.map")
    new_map = Path("src/resources/calendars/calendar-resource.map")
    if not old_map.is_file() or new_map.exists():
        raise SystemExit("resource export map state is not the expected pre-migration state")
    old_map.rename(new_map)

    Path(".github/workflows/calendar-resource-migration.yml").unlink()
    Path(__file__).unlink()


if __name__ == "__main__":
    main()
