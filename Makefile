# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 1993-2026 Shannon Smith

CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config
G_IR_SCANNER ?= g-ir-scanner
G_IR_COMPILER ?= g-ir-compiler
PREFIX ?= /usr
DESTDIR ?=

VERSION := $(strip $(file <VERSION))
UUID := calendar@the-infiltratr
APPLET_SRC_DIR := src/cinnamon
ICON_NAME := infiltratr-calendar
ICON_SRC := src/assets/$(ICON_NAME).png
I18N_DIR := src/i18n
BUILD_DIR := build
DIST_DIR := dist
# Common owns typography family/role metadata only. Calendar deliberately does
# not redistribute proprietary font binaries; Cinnamon/system fallback remains
# authoritative when the preferred MB Corpo families are unavailable.
INFILTRATR_COMMON_DIR := src/vendor/infiltratr-common
INFILTRATR_COMMON_URL := https://github.com/Infiltrator-Projects/Libraries.git
INFILTRATR_COMMON_COMMIT := cfde05efabf55c4f0408ee908034d649303d5a26
INFILTRATR_COMMON_VERSION := 1.19.75
LIB_BASENAME := calendar
LIB_SONAME := lib$(LIB_BASENAME).so.0
LIB_REALNAME := lib$(LIB_BASENAME).so.0.0.0
ABOUT_BINARY := calendar-about
ABOUT_DESKTOP_TEMPLATE := src/app/calendar-about.desktop.in
ABOUT_DESKTOP := $(BUILD_DIR)/calendar-about.desktop
GIR := Calendar-1.0
CORE_SOURCES := \
	src/core/clock-engine.c \
	src/core/time-formats.c \
	src/core/julian-day.c \
	src/core/calendar-registry.c \
	src/core/locale-weekend.c \
	src/core/calendar-core.c \
	src/core/event-core.c \
	src/core/event-source.c
ADAPTER_SOURCES := \
	src/adapters/system-clock.c \
	src/adapters/temporal-policy-source.c \
	src/adapters/clock-glib-adapter.c \
	src/adapters/calendar-system.c \
	src/adapters/calendar-gvariant-adapter.c \
	src/adapters/event-store.c \
	src/adapters/event-gvariant-adapter.c
SOURCES := src/app/project-info.c src/app/version.c $(CORE_SOURCES) $(ADAPTER_SOURCES)
PUBLIC_HEADERS := \
	src/app/version.h \
	src/adapters/system-clock.h \
	src/core/time-formats.h \
	src/core/calendar-types.h \
	src/adapters/calendar-system.h \
	src/core/event-types.h \
	src/adapters/event-store.h
CORE_HEADERS := \
	src/core/clock-engine.h \
	src/core/time-formats.h \
	src/core/julian-day.h \
	src/core/calendar-types.h \
	src/core/calendar-core.h \
	src/core/calendar-internal.h \
	src/core/calendar-registry.h \
	src/core/event-types.h \
	src/core/event-core.h \
	src/core/event-source.h \
	src/core/integer-math.h \
	src/core/locale-weekend.h
PRIVATE_HEADERS := \
	src/app/project-info.h \
	src/adapters/clock-glib-adapter.h \
	src/adapters/system-clock-private.h \
	src/adapters/temporal-policy-source.h \
	src/adapters/calendar-system-private.h \
	src/adapters/event-store-private.h
HEADERS := $(sort $(PUBLIC_HEADERS) $(CORE_HEADERS) $(PRIVATE_HEADERS))
# Common is consumed through its own public static-library build product.
# Calendar deliberately does not enumerate or compile Common's private
# source-file membership.
INFILTRATR_COMMON_HEADERS := \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/core.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/arithmetic.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/timing.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/temporal.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/temporal_posix.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/temporal_runtime.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/temporal_calendar.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/temporal_clock_resource.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/dynlib.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/design.h \
	$(INFILTRATR_COMMON_DIR)/include/infiltratr/utf8.h
# The Common sub-make runs from src/vendor/infiltratr-common. Keep its BUILD_DIR
# textual value relative and whitespace-free so GNU make never has to parse the
# checkout's absolute path as a target name.
INFILTRATR_COMMON_BUILD_DIR := ../../../$(BUILD_DIR)/infiltratr-common
INFILTRATR_COMMON_ARCHIVE := $(BUILD_DIR)/infiltratr-common/libinfiltratr-common.a
INFILTRATR_COMMON_CPPFLAGS := -Iinclude
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
CORE_OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
CORE_ARCHIVE := $(BUILD_DIR)/libcalendar-core.a
# Optional provider modules are built and published by Common, never Calendar.
CALENDAR_RESOURCE_BUILD_DIR ?= /System/Resources/Calendars
CLOCK_RESOURCE_BUILD_DIR ?= /System/Resources/Clocks
TEST_CLOCK_RESOURCE_CPPFLAG := -DCALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY=\"$(CLOCK_RESOURCE_BUILD_DIR)\"
TEST_CALENDAR_RESOURCE_CPPFLAG := -DCALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY=\"$(CALENDAR_RESOURCE_BUILD_DIR)\"
RUNTIME_CALENDAR_FIXTURE_DIR := $(BUILD_DIR)/calendar-resource-fixture
RUNTIME_CALENDAR_FIXTURE := $(RUNTIME_CALENDAR_FIXTURE_DIR)/fixture-calendar.so
MULTIARCH ?= $(shell $(CC) -dumpmachine)
LIBDIR ?= $(PREFIX)/lib/$(MULTIARCH)

CPPFLAGS += -Isrc/app -Isrc/core -Isrc/adapters -I$(INFILTRATR_COMMON_DIR)/include \
	-DCALENDAR_VERSION=\"$(VERSION)\" \
	-DCALENDAR_SOURCE_ID=\"calendar-$(VERSION)\" \
	-DCALENDAR_BUILD_PROFILE=\"$(BUILD_MODE)\" \
	-DGETTEXT_PACKAGE=\"$(UUID)\" \
	-DU_DISABLE_RENAMING=1
BUILD_MODE ?= generic
GENERIC_CFLAGS := -O2 -g
# Apply the same compiler-probed ELF call/visibility optimisations as
# System Monitor to Calendar's C clock/date libraries AND pinned Common.
NATIVE_ELF_FLAGS := $(shell for flag in -fno-semantic-interposition -fno-plt; do \
    printf 'int f(void){return 0;}\n' | $(CC) -std=c11 -x c -c "$$flag" -o /dev/null - \
    >/dev/null 2>&1 && printf '%s ' "$$flag"; done)
NATIVE_CFLAGS := -O3 -g -march=native -mtune=native -flto=auto $(NATIVE_ELF_FLAGS)
PGO_MODE ?= none
PGO_DIR ?= $(abspath .pgo-data)
PGO_GENERATE_CFLAGS := -fprofile-generate=$(PGO_DIR)
PGO_USE_CFLAGS := -fprofile-use=$(PGO_DIR) -fprofile-correction -fprofile-partial-training -Wno-error=missing-profile
PGO_GENERATE_LDFLAGS := -fprofile-generate=$(PGO_DIR)
PGO_USE_LDFLAGS := -fprofile-use=$(PGO_DIR) -fprofile-correction

ifeq ($(PGO_MODE),none)
PGO_CFLAGS :=
PGO_LDFLAGS :=
else ifeq ($(PGO_MODE),generate)
PGO_CFLAGS := $(PGO_GENERATE_CFLAGS)
PGO_LDFLAGS := $(PGO_GENERATE_LDFLAGS)
else ifeq ($(PGO_MODE),use)
PGO_CFLAGS := $(PGO_USE_CFLAGS)
PGO_LDFLAGS := $(PGO_USE_LDFLAGS)
else
$(error Unsupported PGO_MODE '$(PGO_MODE)'; use none, generate or use)
endif

INFILTRATR_COMMON_EXTRA_CFLAGS ?=
SANITIZER_CFLAGS := -fsanitize=address,undefined -fno-omit-frame-pointer
REPRODUCIBLE_CFLAGS := \
	"-ffile-prefix-map=$(CURDIR)=." \
	"-fdebug-prefix-map=$(CURDIR)=." \
	"-fmacro-prefix-map=$(CURDIR)=."
CALENDAR_CFLAGS := -std=c11 -fPIC -Wall -Wextra -Wpedantic -Werror \
	-Wformat=2 -Wshadow -Wstrict-prototypes -Wmissing-prototypes \
	-Wconversion -Wsign-conversion -Wnull-dereference \
	-fstack-protector-strong -fno-common $(REPRODUCIBLE_CFLAGS)

ifeq ($(BUILD_MODE),generic)
CFLAGS ?= $(GENERIC_CFLAGS)
CFLAGS += $(CALENDAR_CFLAGS)
INFILTRATR_COMMON_CFLAGS := $(GENERIC_CFLAGS) -fstack-protector-strong \
	-fno-common $(REPRODUCIBLE_CFLAGS) $(INFILTRATR_COMMON_EXTRA_CFLAGS)
BUILD_DESCRIPTION := generic amd64-compatible (system temporal resource consumer)
else ifeq ($(BUILD_MODE),native)
CFLAGS ?=
override CFLAGS += $(NATIVE_CFLAGS) $(PGO_CFLAGS) $(CALENDAR_CFLAGS)
INFILTRATR_COMMON_CFLAGS := $(NATIVE_CFLAGS) $(PGO_CFLAGS) -fstack-protector-strong \
	-fno-common $(REPRODUCIBLE_CFLAGS) $(INFILTRATR_COMMON_EXTRA_CFLAGS)
LDFLAGS += -flto=auto $(PGO_LDFLAGS)
ifeq ($(PGO_MODE),use)
BUILD_DESCRIPTION := local hardware-native PGO-trained (-O3 -march=native -mtune=native -flto=auto -fprofile-use)
else ifeq ($(PGO_MODE),generate)
BUILD_DESCRIPTION := local hardware-native PGO-instrumented (-O3 -march=native -mtune=native -flto=auto -fprofile-generate)
else
BUILD_DESCRIPTION := local hardware-native (-O3 -march=native -mtune=native -flto=auto)
endif
else
$(error Unsupported BUILD_MODE '$(BUILD_MODE)'; use generic or native)
endif

REPRO_SEED_PREFIX ?= calendar-$(VERSION)
LDFLAGS += -Wl,-z,relro,-z,now -Wl,--as-needed
SHARED_LDFLAGS = $(LDFLAGS) -Wl,--version-script=src/abi/calendar.map
GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags gio-2.0 gobject-2.0 icu-i18n)
GLIB_LIBS = $(shell $(PKG_CONFIG) --libs gio-2.0 gobject-2.0)
GTK_CFLAGS = $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
GTK_LIBS = $(shell $(PKG_CONFIG) --libs gtk+-3.0)
CORE_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0 icu-i18n)
CORE_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0)
DYNLIB_LIBS = -ldl
PLATFORM_LIBS = $(DYNLIB_LIBS) -pthread
MATH_LIBS = -lm
GIR_INCLUDE_PATH ?= $(shell $(PKG_CONFIG) --variable=libdir glib-2.0)/gir-1.0
GIR_SHARE_INCLUDE_PATH ?= $(PREFIX)/share/gir-1.0

SOURCE_DATE_EPOCH ?= 1792713768
COVERAGE_MIN_LINES ?= 80
COVERAGE_MIN_BRANCHES ?= 60
DIST_FILES := \
	.github \
	.gitmodules \
	CHANGELOG.md \
	CONTRIBUTING.md \
	LICENSE \
	Makefile \
	VERSION \
	README.md \
	SECURITY.md \
	debian \
	docs \
	src \
	tests \
	tools

.PHONY: all check check-deps clean common-bootstrap common-build common-check common-test core-check coverage install package-source \
	package-local-installer pgo-train \
	sanitize static-analysis test validate-architecture validate-js validate-package-inputs \
	validate-about-desktop validate-sources validate-exports validate-abi validate-runtime-deps validate-release-model validate-upstream-drift smoke-gjs \
	path-space-smoke release-check \
	reproducible-build translations update-pot validate-translations \
	update-settings validate-settings-generated update-theme validate-theme update-runtime-hashes

all: common-check check-deps \
	$(INFILTRATR_COMMON_ARCHIVE) \
	$(CORE_ARCHIVE) \
	$(BUILD_DIR)/$(LIB_REALNAME) \
	$(BUILD_DIR)/$(GIR).typelib \
	$(BUILD_DIR)/$(ABOUT_BINARY) \
	$(ABOUT_DESKTOP) \
	translations

common-bootstrap: common-check
	@:

common-check:
	@if test ! -f "$(INFILTRATR_COMMON_DIR)/VERSION"; then \
		command -v git >/dev/null 2>&1 || { \
			echo "git is required to retrieve the pinned shared source." >&2; \
			exit 1; \
		}; \
		if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then \
			git submodule update --init --depth 1 -- "$(INFILTRATR_COMMON_DIR)"; \
		else \
			mkdir -p "$(INFILTRATR_COMMON_DIR)"; \
			git -C "$(INFILTRATR_COMMON_DIR)" init -q; \
			git -C "$(INFILTRATR_COMMON_DIR)" remote add origin "$(INFILTRATR_COMMON_URL)"; \
			git -C "$(INFILTRATR_COMMON_DIR)" fetch -q --depth 1 origin "$(INFILTRATR_COMMON_COMMIT)"; \
			git -C "$(INFILTRATR_COMMON_DIR)" checkout -q --detach FETCH_HEAD; \
		fi; \
	fi
	@test -f "$(INFILTRATR_COMMON_DIR)/VERSION" || { \
		echo "Unable to retrieve Libraries $(INFILTRATR_COMMON_VERSION)." >&2; \
		exit 1; \
	}
	@test "$$(tr -d '[:space:]' < "$(INFILTRATR_COMMON_DIR)/VERSION")" = \
		"$(INFILTRATR_COMMON_VERSION)" || { \
		echo "Libraries $(INFILTRATR_COMMON_VERSION) is required." >&2; \
		exit 1; \
	}
	@actual_commit=$$(git -C "$(INFILTRATR_COMMON_DIR)" rev-parse HEAD 2>/dev/null || true); \
		if test -n "$$actual_commit" && test "$$actual_commit" != "$(INFILTRATR_COMMON_COMMIT)"; then \
			echo "Libraries must be pinned to $(INFILTRATR_COMMON_COMMIT)." >&2; \
			exit 1; \
		fi

check-deps: common-check
	@command -v "$(PKG_CONFIG)" >/dev/null || { \
		echo "Missing build dependency: pkg-config" >&2; exit 1; }
	@$(G_IR_SCANNER) --version >/dev/null 2>&1 || { \
		echo "Missing build dependency: g-ir-scanner" >&2; exit 1; }
	@$(G_IR_COMPILER) --version >/dev/null 2>&1 || { \
		echo "Missing build dependency: g-ir-compiler" >&2; exit 1; }
	@$(PKG_CONFIG) --exists gio-2.0 || { \
		echo "Missing build dependency: libglib2.0-dev (gio-2.0)" >&2; exit 1; }
	@$(PKG_CONFIG) --exists gobject-2.0 || { \
		echo "Missing build dependency: libglib2.0-dev" >&2; exit 1; }
	@$(PKG_CONFIG) --exists glib-2.0 || { \
		echo "Missing build dependency: libglib2.0-dev" >&2; exit 1; }
	@$(PKG_CONFIG) --exists icu-i18n || { \
		echo "Missing build dependency: libicu-dev" >&2; exit 1; }
	@$(PKG_CONFIG) --exists gtk+-3.0 || { \
		echo "Missing build dependency: libgtk-3-dev" >&2; exit 1; }
	@command -v msgfmt >/dev/null || { \
		echo "Missing build dependency: gettext" >&2; exit 1; }
	@command -v python3 >/dev/null || { \
		echo "Missing test dependency: python3" >&2; exit 1; }
	@command -v node >/dev/null || { \
		echo "Missing test dependency: nodejs" >&2; exit 1; }
	@command -v gjs >/dev/null || { \
		echo "Missing test dependency: gjs" >&2; exit 1; }
	@command -v xz >/dev/null || { \
		echo "Missing build dependency: xz" >&2; exit 1; }

$(INFILTRATR_COMMON_HEADERS): | common-check
	@test -f "$@" || { echo "Unable to materialize pinned Libraries public header: $@" >&2; exit 1; }

$(BUILD_DIR)/%.o: src/%.c $(HEADERS)
	@mkdir -p "$(dir $@)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-$(notdir $<) $(GLIB_CFLAGS) -MMD -MP -c $< -o $@

common-build: common-check $(INFILTRATR_COMMON_HEADERS)
	$(MAKE) -C "$(INFILTRATR_COMMON_DIR)" \
		CC="$(CC)" AR="$(AR)" \
		CPPFLAGS="$(INFILTRATR_COMMON_CPPFLAGS)" \
		CFLAGS='$(INFILTRATR_COMMON_CFLAGS)' \
		BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)" all
	@test -f "$(INFILTRATR_COMMON_ARCHIVE)" || { \
		echo "Libraries did not produce $(INFILTRATR_COMMON_ARCHIVE)" >&2; \
		exit 1; \
	}

$(INFILTRATR_COMMON_ARCHIVE): common-build
	@:

common-test: $(INFILTRATR_COMMON_ARCHIVE)
	$(MAKE) -C "$(INFILTRATR_COMMON_DIR)" \
		CC="$(CC)" AR="$(AR)" \
		CPPFLAGS="$(INFILTRATR_COMMON_CPPFLAGS)" \
		CFLAGS='$(INFILTRATR_COMMON_CFLAGS)' \
		BUILD_DIR="$(INFILTRATR_COMMON_BUILD_DIR)" check

$(CORE_ARCHIVE): $(CORE_OBJECTS)
	@rm -f "$@"
	$(AR) rcsD "$@" $(CORE_OBJECTS)

$(BUILD_DIR)/$(LIB_REALNAME): $(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE) \
		src/abi/calendar.map
	$(CC) -shared $(SHARED_LDFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-link -Wl,-soname,$(LIB_SONAME) -o $@ \
		$(OBJECTS) $(INFILTRATR_COMMON_ARCHIVE) $(GLIB_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)
	ln -sfn "$(LIB_REALNAME)" "$(BUILD_DIR)/$(LIB_SONAME)"
	ln -sfn "$(LIB_SONAME)" "$(BUILD_DIR)/lib$(LIB_BASENAME).so"

$(BUILD_DIR)/pgo-train: tools/pgo-train.c $(BUILD_DIR)/$(LIB_REALNAME)
	$(CC) -O2 -g -std=c11 -Isrc/core -Isrc/adapters $(GLIB_CFLAGS) \
		tools/pgo-train.c -L"$(abspath $(BUILD_DIR))" -l$(LIB_BASENAME) \
		$(GLIB_LIBS) $(MATH_LIBS) -o $@

pgo-train: $(BUILD_DIR)/pgo-train
	@test "$(BUILD_MODE)" = "native" || { \
		echo "PGO training requires BUILD_MODE=native." >&2; exit 1; }
	@test "$(PGO_MODE)" = "generate" || { \
		echo "PGO training requires PGO_MODE=generate." >&2; exit 1; }
	LD_LIBRARY_PATH="$(abspath $(BUILD_DIR))" \
		./$(BUILD_DIR)/pgo-train

$(BUILD_DIR)/$(ABOUT_BINARY): src/app/about-dialog.c src/app/project-info.c \
		src/app/project-info.h $(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GTK_CFLAGS) \
		-frandom-seed=$(REPRO_SEED_PREFIX)-about-dialog \
		src/app/about-dialog.c src/app/project-info.c \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(GTK_LIBS) $(MATH_LIBS)
	chmod 0755 $@

$(ABOUT_DESKTOP): $(ABOUT_DESKTOP_TEMPLATE)
	@mkdir -p "$(BUILD_DIR)"
	@sed 's|@ABOUT_EXEC@|$(PREFIX)/libexec/$(ABOUT_BINARY)|g' "$<" > "$@"

translations:
	@mkdir -p "$(BUILD_DIR)/locale"
	@while IFS= read -r language; do \
		case "$$language" in ''|'#'*) continue ;; esac; \
		mkdir -p "$(BUILD_DIR)/locale/$$language/LC_MESSAGES"; \
		msgfmt --check --statistics \
			-o "$(BUILD_DIR)/locale/$$language/LC_MESSAGES/$(UUID).mo" \
			"$(I18N_DIR)/$$language.po"; \
	done < $(I18N_DIR)/LINGUAS

update-pot:
	python3 tools/update-translations.py

validate-translations:
	python3 tools/update-translations.py --check

update-settings:
	python3 tools/update-settings.py

validate-settings-generated:
	python3 tools/update-settings.py --check
	python3 tools/update-theme-css.py --check

update-theme:
	python3 tools/update-theme-css.py

validate-theme:
	python3 tools/update-theme-css.py --check

$(BUILD_DIR)/$(GIR).gir: \
		$(PUBLIC_HEADERS) \
		$(SOURCES) \
		$(BUILD_DIR)/$(LIB_REALNAME)
	@mkdir -p "$(BUILD_DIR)/tmp"
	LD_LIBRARY_PATH="$(abspath $(BUILD_DIR))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" \
	TMPDIR="$(abspath $(BUILD_DIR)/tmp)" \
	CFLAGS= \
	$(G_IR_SCANNER) \
		--quiet \
		--warn-all \
		--no-libtool \
		--namespace=Calendar \
		--nsversion=1.0 \
		--identifier-prefix=Calendar \
		--symbol-prefix=calendar \
		--add-include-path="$(GIR_INCLUDE_PATH)" \
		--add-include-path="$(GIR_SHARE_INCLUDE_PATH)" \
		--include=GLib-2.0 \
		--include=GObject-2.0 \
		--library=$(LIB_BASENAME) \
		--library-path="$(abspath $(BUILD_DIR))" \
		--cflags-begin $(CPPFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-gir $(GLIB_CFLAGS) --cflags-end \
		--output=$@ \
		$(PUBLIC_HEADERS) $(SOURCES)

$(BUILD_DIR)/$(GIR).typelib: $(BUILD_DIR)/$(GIR).gir
	$(G_IR_COMPILER) \
		--includedir="$(GIR_INCLUDE_PATH)" \
		--includedir="$(GIR_SHARE_INCLUDE_PATH)" \
		$< \
		--output=$@

$(BUILD_DIR)/test-time-formats: \
		tests/test-time-formats.c \
		$(SOURCES) \
		$(HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(TEST_CALENDAR_RESOURCE_CPPFLAG) $(TEST_CLOCK_RESOURCE_CPPFLAG) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-time-formats $(GLIB_CFLAGS) \
		tests/test-time-formats.c $(SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(GLIB_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-properties: \
		tests/test-properties.c \
		$(SOURCES) \
		$(HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(TEST_CALENDAR_RESOURCE_CPPFLAG) $(TEST_CLOCK_RESOURCE_CPPFLAG) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-properties $(GLIB_CFLAGS) \
		tests/test-properties.c $(SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(GLIB_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-portable-core: \
		tests/test-portable-core.c \
		$(CORE_SOURCES) \
		$(CORE_HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(TEST_CALENDAR_RESOURCE_CPPFLAG) $(TEST_CLOCK_RESOURCE_CPPFLAG) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-portable-core $(CORE_CFLAGS) \
		tests/test-portable-core.c $(CORE_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(CORE_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-exact-clock-boundaries: \
		tests/test-exact-clock-boundaries.c \
		$(CORE_SOURCES) \
		$(CORE_HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(TEST_CALENDAR_RESOURCE_CPPFLAG) $(TEST_CLOCK_RESOURCE_CPPFLAG) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-exact-clock-boundaries $(CORE_CFLAGS) \
		tests/test-exact-clock-boundaries.c $(CORE_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(CORE_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

$(RUNTIME_CALENDAR_FIXTURE): \
		tests/calendar-resource-fixture.c \
		$(INFILTRATR_COMMON_HEADERS)
	@mkdir -p "$(RUNTIME_CALENDAR_FIXTURE_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-calendar-resource-fixture \
		$(CORE_CFLAGS) -shared tests/calendar-resource-fixture.c \
		-o $@ $(LDFLAGS) $(CORE_LIBS)

# Canonical native filenames contain spaces, so resource files cannot be
# represented as an unquoted GNU Make wildcard/prerequisite list. Stage the
# installed resource directory atomically as a qualified package unit.
.PHONY: stage-runtime-calendar-resources
stage-runtime-calendar-resources: $(RUNTIME_CALENDAR_FIXTURE)
	@find "$(RUNTIME_CALENDAR_FIXTURE_DIR)" -maxdepth 1 -type f -name '*.so' ! -name 'fixture-calendar.so' -delete
	@cp -a "$(CALENDAR_RESOURCE_BUILD_DIR)/." "$(RUNTIME_CALENDAR_FIXTURE_DIR)/"
	@find "$(CALENDAR_RESOURCE_BUILD_DIR)" -maxdepth 1 -type f -name '*.so' | awk 'END { if (NR != 31) { print "Expected 31 installed calendar modules"; exit 1 } }'
	@find "$(RUNTIME_CALENDAR_FIXTURE_DIR)" -maxdepth 1 -type f -name '*.so' ! -name 'fixture-calendar.so' | awk 'END { if (NR != 31) { print "Expected 31 staged calendar modules"; exit 1 } }'

$(BUILD_DIR)/test-runtime-calendar-resource: \
		tests/test-runtime-calendar-resource.c \
		$(RUNTIME_CALENDAR_FIXTURE) \
		$(CORE_SOURCES) \
		$(CORE_HEADERS) \
		$(INFILTRATR_COMMON_ARCHIVE) | stage-runtime-calendar-resources
	@mkdir -p "$(BUILD_DIR)"
	$(CC) $(CPPFLAGS) $(CFLAGS) \
		-DCALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY=\"$(abspath $(RUNTIME_CALENDAR_FIXTURE_DIR))\" \
		-frandom-seed=$(REPRO_SEED_PREFIX)-test-runtime-calendar-resource $(CORE_CFLAGS) \
		tests/test-runtime-calendar-resource.c $(CORE_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(CORE_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-missing-clock-resources: \
		tests/test-missing-clock-resources.c \
		$(CORE_SOURCES) $(CORE_HEADERS) $(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)/empty-clock-resources"
	@if find "$(BUILD_DIR)/empty-clock-resources" -maxdepth 1 -name '*.so' -print -quit | grep -q .; then \
		echo "Missing-resource fixture unexpectedly contains a clock module" >&2; exit 1; fi
	$(CC) $(CPPFLAGS) \
		-DCALENDAR_NATIVE_CLOCK_RESOURCE_DIRECTORY=\"$(abspath $(BUILD_DIR)/empty-clock-resources)\" \
		$(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-missing-clock-resources \
		$(CORE_CFLAGS) tests/test-missing-clock-resources.c $(CORE_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(CORE_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

$(BUILD_DIR)/test-missing-calendar-resources: \
		tests/test-missing-calendar-resources.c \
		$(CORE_SOURCES) $(CORE_HEADERS) $(INFILTRATR_COMMON_ARCHIVE)
	@mkdir -p "$(BUILD_DIR)/empty-calendar-resources"
	@if find "$(BUILD_DIR)/empty-calendar-resources" -maxdepth 1 -name '*.so' -print -quit | grep -q .; then \
		echo "Missing-calendar fixture unexpectedly contains a resource module" >&2; exit 1; fi
	$(CC) $(CPPFLAGS) \
		-DCALENDAR_NATIVE_CALENDAR_RESOURCE_DIRECTORY=\"$(abspath $(BUILD_DIR)/empty-calendar-resources)\" \
		$(CFLAGS) -frandom-seed=$(REPRO_SEED_PREFIX)-test-missing-calendar-resources \
		$(CORE_CFLAGS) tests/test-missing-calendar-resources.c $(CORE_SOURCES) \
		$(INFILTRATR_COMMON_ARCHIVE) -o $@ $(LDFLAGS) $(CORE_LIBS) \
		$(PLATFORM_LIBS) $(MATH_LIBS)

test: $(BUILD_DIR)/test-time-formats $(BUILD_DIR)/test-properties \
	$(BUILD_DIR)/test-missing-clock-resources \
	$(BUILD_DIR)/test-missing-calendar-resources \
	$(BUILD_DIR)/test-portable-core \
	$(BUILD_DIR)/test-exact-clock-boundaries \
	$(BUILD_DIR)/test-runtime-calendar-resource \
	common-test \
	$(BUILD_DIR)/$(ABOUT_BINARY)
	./$(BUILD_DIR)/test-missing-clock-resources
	./$(BUILD_DIR)/test-missing-calendar-resources
	./$(BUILD_DIR)/test-time-formats
	./$(BUILD_DIR)/test-properties
	./$(BUILD_DIR)/test-portable-core
	./$(BUILD_DIR)/test-exact-clock-boundaries
	./$(BUILD_DIR)/test-runtime-calendar-resource
	@./$(BUILD_DIR)/$(ABOUT_BINARY) --print-metadata | \
		grep -qx 'version=$(VERSION)'
	@./$(BUILD_DIR)/$(ABOUT_BINARY) --print-metadata | \
		grep -qx 'common-library=infiltratr-common-$(INFILTRATR_COMMON_VERSION)'

validate-js: validate-settings-generated
	python3 tools/update-runtime-loader.py --check
	@python3 -c 'import json; \
	from pathlib import Path; \
	p=Path("$(APPLET_SRC_DIR)"); \
	[json.load(open(p / n, encoding="utf-8")) for n in ("metadata.json", "settings-schema.json")]; \
	assert json.load(open(p / "metadata.json", encoding="utf-8"))["uuid"] == "$(UUID)"'
	python3 tests/test-settings.py
	node tests/test-popup-menu.js
	node tests/test-native-boundary.js
	node tests/test-temporal-policy.js
	node tests/test-calendar-server-transport.js
	node tests/test-event-manager.js
	node tests/test-js-runtime.js
	@for source in "$(APPLET_SRC_DIR)"/*.js; do node --check "$$source"; done
	@! grep -R -n -E 'const UUID = "calendar@cinnamon\.org"' "$(APPLET_SRC_DIR)" || { \
		echo "Stock applet UUID is configured as the runtime identity." >&2; exit 1; }

validate-exports: $(BUILD_DIR)/$(LIB_REALNAME)
	@command -v nm >/dev/null || { \
		echo "Missing validation dependency: nm" >&2; exit 1; }
	@nm -D --defined-only --format=posix "$(BUILD_DIR)/$(LIB_REALNAME)" | \
		awk '$$1 ~ /^calendar_/ { sub(/@.*/, "", $$1); print $$1 }' | LC_ALL=C sort -u > \
		"$(BUILD_DIR)/exported-symbols.actual"
	@sed -n 's/^[[:space:]]*\(calendar_[A-Za-z0-9_]*\);[[:space:]]*$$/\1/p' \
		src/abi/calendar.map | LC_ALL=C sort -u > \
		"$(BUILD_DIR)/exported-symbols.expected"
	diff -u "$(BUILD_DIR)/exported-symbols.expected" \
		"$(BUILD_DIR)/exported-symbols.actual"

validate-runtime-deps: $(BUILD_DIR)/$(LIB_REALNAME)
	@command -v readelf >/dev/null || { \
		echo "Missing validation dependency: readelf" >&2; exit 1; }
	@for binary in "$(BUILD_DIR)/$(LIB_REALNAME)"; do \
		needed=$$(readelf -d "$$binary" | \
			sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p'); \
		if printf '%s\n' "$$needed" | grep -Eq '^libicu(i18n|uc|data)\.so'; then \
			echo "Generic runtime must not embed an ICU-major DT_NEEDED entry in $$binary:" >&2; \
			printf '%s\n' "$$needed" >&2; \
			exit 1; \
		fi; \
	done


smoke-gjs: $(BUILD_DIR)/$(GIR).typelib
	LD_LIBRARY_PATH="$(abspath $(BUILD_DIR))$${LD_LIBRARY_PATH:+:$$LD_LIBRARY_PATH}" \
	GI_TYPELIB_PATH="$(abspath $(BUILD_DIR))$${GI_TYPELIB_PATH:+:$$GI_TYPELIB_PATH}" \
	CALENDAR_EXPECTED_VERSION="$(VERSION)" \
		gjs tests/smoke-typelib.js

update-runtime-hashes:
	python3 tools/update-runtime-hashes.py

validate-sources:
	python3 tools/update-runtime-hashes.py --check
	sha256sum --check "$(APPLET_SRC_DIR)/runtime-sources.sha256"

path-space-smoke:
	tools/path-space-build.sh

validate-package-inputs:
	python3 tools/validate-package-inputs.py

validate-about-desktop: $(ABOUT_DESKTOP)
	@command -v desktop-file-validate >/dev/null || { \
		echo "Missing validation dependency: desktop-file-validate" >&2; exit 1; }
	desktop-file-validate "$(ABOUT_DESKTOP)"

validate-release-model:
	python3 tests/test-release-model.py

validate-architecture:
	python3 tests/test-architecture.py

validate-upstream-drift:
	python3 tests/test-upstream-drift.py

core-check: check-deps $(CORE_ARCHIVE) $(BUILD_DIR)/test-portable-core \
	$(BUILD_DIR)/test-exact-clock-boundaries validate-architecture
	./$(BUILD_DIR)/test-portable-core
	./$(BUILD_DIR)/test-exact-clock-boundaries

check: all test validate-js validate-translations validate-sources validate-package-inputs \
	validate-about-desktop validate-release-model validate-architecture validate-upstream-drift validate-exports validate-abi validate-runtime-deps smoke-gjs path-space-smoke

sanitize:
	$(MAKE) clean
	ASAN_OPTIONS="detect_leaks=0:halt_on_error=1" \
	UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
	$(MAKE) CFLAGS="-O1 -g -std=c11 -fPIC -Wall -Wextra -Wpedantic \
		-Werror -Wformat=2 -Wshadow -Wstrict-prototypes \
		-Wmissing-prototypes $(SANITIZER_CFLAGS)" \
		INFILTRATR_COMMON_EXTRA_CFLAGS="$(SANITIZER_CFLAGS)" \
		LDFLAGS="-fsanitize=address,undefined" test
	@nm -u "$(INFILTRATR_COMMON_ARCHIVE)" | grep -Eq '__asan_|__ubsan_' || { \
		echo "Sanitizer instrumentation is missing from Libraries." >&2; \
		exit 1; }
	@for test_path in \
		/portable/clock-interfaces \
		/portable/clock-destroy-during-tick \
		/portable/event-source \
		/portable/time-catalogue; do \
		ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
		UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
			./$(BUILD_DIR)/test-portable-core -p "$$test_path"; \
	done

coverage:
	$(MAKE) clean
	$(MAKE) CFLAGS="-O0 -g -std=c11 -fPIC -Wall -Wextra -Wpedantic \
		-Werror -Wformat=2 -Wshadow -Wstrict-prototypes \
		-Wmissing-prototypes --coverage" \
		LDFLAGS="--coverage" test
	@command -v gcovr >/dev/null || { \
		echo "gcovr is required for the coverage gate" >&2; exit 1; }
	gcovr --root . --filter src --exclude 'src/app/about-dialog.c' --exclude 'src/vendor/' \
		--print-summary --fail-under-line $(COVERAGE_MIN_LINES) \
		--fail-under-branch $(COVERAGE_MIN_BRANCHES) \
		--html-details -o "$(BUILD_DIR)/coverage.html" $(BUILD_DIR)

# GNU libc does not implement optional Annex K *_s functions. Exclude only
# that migration advisory; bounds, lifetime and other analyzer checks remain.
static-analysis:
	@command -v clang-tidy >/dev/null || { \
		echo "clang-tidy is not installed" >&2; exit 1; }
	clang-tidy \
		-checks='-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-reserved-identifier,performance-*,portability-*' \
		-warnings-as-errors='clang-analyzer-*,bugprone-*,performance-*,portability-*' \
		$(SOURCES) -- $(CPPFLAGS) $(GLIB_CFLAGS) \
		-std=c11 -Wall -Wextra -Wpedantic
	# The About helper is a separately linked production executable. Analyse it
	# with its GTK boundary flags instead of contaminating the library source set.
	clang-tidy \
		-checks='-*,clang-analyzer-*,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,bugprone-*,-bugprone-easily-swappable-parameters,-bugprone-reserved-identifier,performance-*,portability-*' \
		-warnings-as-errors='clang-analyzer-*,bugprone-*,performance-*,portability-*' \
		src/app/about-dialog.c -- $(CPPFLAGS) $(GTK_CFLAGS) \
		-std=c11 -Wall -Wextra -Wpedantic

install: all
	install -Dm755 "$(BUILD_DIR)/$(LIB_REALNAME)" \
		"$(DESTDIR)$(LIBDIR)/$(LIB_REALNAME)"
	ln -sfn "$(LIB_REALNAME)" \
		"$(DESTDIR)$(LIBDIR)/$(LIB_SONAME)"
	install -Dm644 "$(BUILD_DIR)/$(GIR).typelib" \
		"$(DESTDIR)$(LIBDIR)/girepository-1.0/$(GIR).typelib"
	install -Dm755 "$(BUILD_DIR)/$(ABOUT_BINARY)" \
		"$(DESTDIR)$(PREFIX)/libexec/$(ABOUT_BINARY)"
	install -Dm644 "$(ABOUT_DESKTOP)" \
		"$(DESTDIR)$(PREFIX)/share/applications/calendar-about.desktop"
	install -Dm644 "$(ICON_SRC)" \
		"$(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/$(ICON_NAME).png"
	install -Dm644 "$(ICON_SRC)" \
		"$(DESTDIR)$(PREFIX)/share/app-install/icons/calendar.png"
	install -d \
		"$(DESTDIR)$(PREFIX)/share/cinnamon/applets/$(UUID)" \
		"$(DESTDIR)$(PREFIX)/share/infiltrator/policy-providers"
	install -m644 "$(ICON_SRC)" \
		"$(DESTDIR)$(PREFIX)/share/cinnamon/applets/$(UUID)/icon.png"
	install -m644 "$(APPLET_SRC_DIR)"/*.js "$(APPLET_SRC_DIR)"/*.json \
		"$(APPLET_SRC_DIR)"/*.css \
		"$(DESTDIR)$(PREFIX)/share/cinnamon/applets/$(UUID)/"
	@while IFS= read -r language; do \
		case "$$language" in ''|'#'*) continue ;; esac; \
		install -Dm644 \
			"$(BUILD_DIR)/locale/$$language/LC_MESSAGES/$(UUID).mo" \
			"$(DESTDIR)$(PREFIX)/share/locale/$$language/LC_MESSAGES/$(UUID).mo"; \
	done < $(I18N_DIR)/LINGUAS
	install -Dm644 LICENSE \
		"$(DESTDIR)$(PREFIX)/share/licenses/calendar/LICENSE"
	install -d "$(DESTDIR)$(PREFIX)/share/doc/calendar"
	install -m644 README.md \
		"$(DESTDIR)$(PREFIX)/share/doc/calendar/"
	printf 'Calendar source version: %s\nBuild mode: %s\nShared C library: Libraries %s\n' \
		"$(VERSION)" "$(BUILD_DESCRIPTION)" "$(INFILTRATR_COMMON_VERSION)" \
		> "$(BUILD_DIR)/BUILD-INFO"
	install -m644 "$(BUILD_DIR)/BUILD-INFO" \
		"$(DESTDIR)$(PREFIX)/share/doc/calendar/BUILD-INFO"

package-source: common-check validate-settings-generated
	@mkdir -p "$(DIST_DIR)"
	@rm -rf "$(BUILD_DIR)/source-stage"
	@mkdir -p "$(BUILD_DIR)/source-stage/Calendar-$(VERSION)"
	@tar --exclude='.git' --exclude='*/.git' -cf - $(DIST_FILES) | \
		tar -C "$(BUILD_DIR)/source-stage/Calendar-$(VERSION)" -xf -
	@rm -rf \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/.debhelper" \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/calendar" \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/calendar-dbgsym" \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/infiltrator-calendar-resources"
	@rm -f \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/files" \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/"*.substvars \
		"$(BUILD_DIR)/source-stage/Calendar-$(VERSION)/debian/debhelper-build-stamp"
	@find "$(BUILD_DIR)/source-stage" -depth -type d -name __pycache__ -exec rm -rf {} +
	@find "$(BUILD_DIR)/source-stage" -type f \
		\( -name '*.pyc' -o -name '*.pyo' \) -delete
	@find "$(BUILD_DIR)/source-stage" -exec \
		touch --date="@$(SOURCE_DATE_EPOCH)" {} +
	tar --sort=name \
		--mtime="@$(SOURCE_DATE_EPOCH)" \
		--owner=0 --group=0 --numeric-owner \
		-C "$(BUILD_DIR)/source-stage" \
		-cf - "Calendar-$(VERSION)" | \
		gzip -n -9 > \
		"$(BUILD_DIR)/Calendar-$(VERSION)-local-source.tar.gz"
	@rm -rf "$(BUILD_DIR)/source-stage"

package-local-installer: package-source
	tools/build-local-installer.sh \
		"$(BUILD_DIR)/Calendar-$(VERSION)-local-source.tar.gz" \
		"$(DIST_DIR)/calendar-$(VERSION)-local-folder.run"

release-check:
	tools/release-check.sh

reproducible-build:
	tools/reproducible-build.sh

clean:
	rm -rf "$(BUILD_DIR)"

-include $(OBJECTS:.o=.d)
