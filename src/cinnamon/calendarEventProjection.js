// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * Project a native calendar grid onto the narrow event-source port.
 *
 * The month view should render grid actors, not also own CalendarServer cache
 * range selection. This helper derives the representable local range, updates
 * the event port and returns detached per-cell colour arrays for presentation.
 */

var CalendarEventProjection = Object.freeze({
    build(records, eventSource, eventsEnabled, forceReload,
          representableLocalDate, maximumEventDots) {
        const colorsByCell = Array.from({ length: records.length }, () => []);
        if (records.length === 0) {
            return colorsByCell;
        }

        let firstDate = null;
        let lastDate = null;
        let firstRepresentableIndex = -1;

        for (let index = 0; index < records.length; index++) {
            const record = records[index];
            const candidate = representableLocalDate(
                record.year, record.month, record.day
            );
            if (candidate !== null) {
                firstDate = candidate;
                firstRepresentableIndex = index;
                break;
            }
        }
        for (let index = records.length - 1; index >= 0; index--) {
            const record = records[index];
            const candidate = representableLocalDate(
                record.year, record.month, record.day
            );
            if (candidate !== null) {
                lastDate = candidate;
                break;
            }
        }

        if (firstDate === null || lastDate === null) {
            global.logError(
                "Calendar: visible range has no representable local instant."
            );
            return colorsByCell;
        }

        eventSource.set_visible_range(firstDate, lastDate, forceReload);
        if (!eventsEnabled || firstRepresentableIndex < 0) {
            return colorsByCell;
        }

        const colors = eventSource.get_colors_for_range(
            firstDate,
            records.length - firstRepresentableIndex,
            maximumEventDots
        );
        for (let index = 0;
             index < colors.length &&
             firstRepresentableIndex + index < records.length;
             index++) {
            colorsByCell[firstRepresentableIndex + index] = colors[index];
        }
        return colorsByCell;
    },
});
