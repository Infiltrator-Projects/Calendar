// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Named JavaScript boundary for native GVariant contracts.
 *
 * Only this module knows positional native tuple layouts. Presentation and
 * controller modules consume named objects so native ABI evolution cannot
 * scatter field-number assumptions through the Cinnamon surface.
 */

function _unpack(variant, contract) {
    if (variant === null || typeof variant.deep_unpack !== "function") {
        throw new Error(`${contract}: missing GVariant value`);
    }
    return variant.deep_unpack();
}

function _array(value, length, contract) {
    if (!Array.isArray(value) || value.length !== length) {
        throw new Error(`${contract}: unexpected native record shape`);
    }
    return value;
}

var NativeBoundary = Object.freeze({
    decodeCivilDate(variant) {
        const row = _array(_unpack(variant, "civil date"), 3, "civil date");
        return Object.freeze({ year: row[0], month: row[1], day: row[2] });
    },

    decodeTemporalPolicy(variant) {
        const row = _array(
            _unpack(variant, "temporal policy"),
            8,
            "temporal policy"
        );
        return Object.freeze({
            mode: row[0],
            calendar: row[1],
            showSeconds: row[2],
            locationConfigured: row[3],
            latitude: row[4],
            longitude: row[5],
            authority: row[6],
            providerAvailable: row[7],
        });
    },

    decodeCalendarGrid(variant) {
        const rows = _unpack(variant, "calendar grid");
        if (!Array.isArray(rows)) {
            throw new Error("calendar grid: expected row array");
        }
        return rows.map((value) => {
            const row = _array(value, 13, "calendar grid row");
            return Object.freeze({
                dayLabel: row[0],
                year: row[1],
                month: row[2],
                day: row[3],
                row: row[4],
                column: row[5],
                weekNumber: row[6],
                isWorkDay: row[7],
                isToday: row[8],
                isSelected: row[9],
                isCurrentPeriod: row[10],
                isTopRow: row[11],
                isLeftEdge: row[12],
            });
        });
    },

    decodeEventTiming(variant) {
        const row = _array(
            _unpack(variant, "event timing"),
            3,
            "event timing"
        );
        return Object.freeze({
            state: row[0],
            secondsUntilStart: row[1],
            secondsUntilEnd: row[2],
        });
    },

    decodeEventSnapshot(variant) {
        const snapshot = _array(
            _unpack(variant, "event snapshot"),
            2,
            "event snapshot"
        );
        if (!Array.isArray(snapshot[1])) {
            throw new Error("event snapshot: expected event row array");
        }
        return Object.freeze({
            revision: snapshot[0],
            events: snapshot[1].map((value) => {
                const row = _array(value, 10, "event snapshot row");
                return Object.freeze({
                    id: row[0],
                    color: row[1],
                    summary: row[2],
                    allDay: row[3],
                    multiDay: row[4],
                    startUnix: row[5],
                    endUnix: row[6],
                    startDayUnix: row[7],
                    endDayUnix: row[8],
                    modified: row[9],
                });
            }),
        });
    },
});
