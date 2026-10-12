// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Date-only Cinnamon boundary.
 *
 * JavaScript Date is permitted only as a representable local civil carrier.
 * Native chronology remains authoritative. CivilDate preserves valid dates
 * that the host timezone cannot represent as an instant, such as a wholly
 * skipped local date.
 */

const CalendarPlus = imports.gi.CalendarPlus;

class CivilDate {
    constructor(year, month, day) {
        this._year = year;
        this._month = month;
        this._day = day;
    }

    getFullYear() { return this._year; }
    getMonth() { return this._month - 1; }
    getDate() { return this._day; }
    getDay() {
        return CalendarPlus.date_weekday(this._year, this._month, this._day);
    }
}

function fields(date) {
    if (date === null ||
        typeof date.getFullYear !== "function" ||
        typeof date.getMonth !== "function" ||
        typeof date.getDate !== "function") {
        return [NaN, NaN, NaN];
    }
    return [date.getFullYear(), date.getMonth() + 1, date.getDate()];
}

function same(a, b) {
    return CalendarPlus.date_same(...fields(a), ...fields(b));
}

function isValid(year, month, day) {
    return Number.isInteger(year) &&
        year >= -2147483648 && year <= 2147483647 &&
        Number.isInteger(month) &&
        Number.isInteger(day) &&
        CalendarPlus.date_same(year, month, day, year, month, day);
}

function representableLocalDate(year, month, day) {
    if (!isValid(year, month, day)) {
        return null;
    }

    const value = new Date();
    value.setHours(12, 0, 0, 0);
    value.setFullYear(year, month - 1, day);
    if (!Number.isFinite(value.getTime()) ||
        value.getFullYear() !== year ||
        value.getMonth() + 1 !== month ||
        value.getDate() !== day) {
        return null;
    }
    return value;
}

function create(year, month, day) {
    if (!isValid(year, month, day)) {
        return null;
    }
    return representableLocalDate(year, month, day) ||
        new CivilDate(year, month, day);
}

function clone(date) {
    const [year, month, day] = fields(date);
    return create(year, month, day);
}

function fromVariant(parts, nativeBoundary) {
    if (parts === null) {
        return null;
    }
    const civil = nativeBoundary.decodeCivilDate(parts);
    return create(civil.year, civil.month, civil.day);
}

var CivilDateBoundary = Object.freeze({
    fields,
    same,
    isValid,
    representableLocalDate,
    create,
    clone,
    fromVariant,
});
