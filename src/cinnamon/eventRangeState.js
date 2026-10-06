// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 1993-2026 Shannon Smith

/*
 * CalendarServer visible-range request state.
 *
 * This object deliberately contains no D-Bus, event-store or presentation
 * code. It makes request-generation, admission and retry transitions atomic
 * so EventsManager cannot accidentally update one flag without the others.
 */

var EventRangeState = class EventRangeState {
    constructor() {
        this._generation = 0;
        this._pending = false;
        this._succeeded = false;
        this._acceptingEvents = false;
        this._queuedForce = false;
        this._retryAttempt = 0;
    }

    get generation() { return this._generation; }
    get pending() { return this._pending; }
    get succeeded() { return this._succeeded; }
    get acceptingEvents() { return this._acceptingEvents; }
    get queuedForce() { return this._queuedForce; }

    beginRequest() {
        if (this._pending) {
            return 0;
        }
        this._generation += 1;
        this._pending = true;
        this._succeeded = false;
        this._acceptingEvents = false;
        return this._generation;
    }

    isCurrent(generation) {
        return generation === this._generation;
    }

    finishRequest(generation, succeeded) {
        if (!this.isCurrent(generation)) {
            return false;
        }
        this._pending = false;
        this._succeeded = Boolean(succeeded);
        this._acceptingEvents = false;
        return true;
    }

    acceptCompletedRequest() {
        this._acceptingEvents = !this._pending && this._succeeded;
        return this._acceptingEvents;
    }

    closeAdmission() {
        this._acceptingEvents = false;
    }

    invalidate() {
        this._acceptingEvents = false;
        this._succeeded = false;
        return this._pending;
    }

    needsRetry() {
        return !this._pending && !this._succeeded;
    }

    queueForce(force = true) {
        this._queuedForce = this._queuedForce || Boolean(force);
    }

    consumeQueuedForce() {
        const queued = this._queuedForce;
        this._queuedForce = false;
        return queued;
    }

    clearQueuedForce() {
        this._queuedForce = false;
    }

    nextRetryDelay() {
        const delay = Math.min(60, Math.pow(2, Math.min(this._retryAttempt, 5)));
        this._retryAttempt += 1;
        return delay;
    }

    resetRetry() {
        this._retryAttempt = 0;
    }

    resetForServerLoss() {
        this._generation += 1;
        this._pending = false;
        this._succeeded = false;
        this._acceptingEvents = false;
        this._queuedForce = false;
        this._retryAttempt = 0;
    }

    destroy() {
        this.resetForServerLoss();
    }
};
