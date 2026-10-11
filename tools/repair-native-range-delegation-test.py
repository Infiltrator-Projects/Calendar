#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test-js-runtime.js"
source = path.read_text(encoding="utf-8")

old = '''                                add_or_update_in_range(event, timestamp) {
                                    observations.addOrUpdateCalls.push(
                                        [event, timestamp]
                                    );
                                    return true;
                                },
'''
new = '''                                add_or_update_in_range(
                                    event, timestamp, rangeStart, rangeEnd
                                ) {
                                    observations.addOrUpdateCalls.push(
                                        [event, timestamp, rangeStart, rangeEnd]
                                    );
                                    return true;
                                },
'''
assert source.count(old) == 1, "expected one native range-admission mock"
source = source.replace(old, new, 1)

old = '''function testRangeAdmissionRejectsLateOutOfRangeSignals() {
    const { EventsManager, observations } = evaluateEventsManager();
    const manager = new EventsManager();
    manager._inited = true;
    manager.current_range_start = {
        to_unix() { return 1000; },
        add_days() { return this; },
    };
    manager.current_range_end = {
        to_unix() { return 2000; },
        add_days() {
            return { to_unix() { return 3000; } };
        },
    };
    manager._rangeState.finishRequest(manager._rangeState.beginRequest(), true);
    manager._rangeState.acceptCompletedRequest();

    function eventVariant(start, end) {
        return {
            deep_unpack() {
                return ["id", "#112233", "event", false, start, end, 10];
            },
        };
    }
    manager._ingestEvents({
        unpack() {
            return [
                eventVariant(100, 200),
                eventVariant(1500, 1600),
            ];
        },
    });
    assert.equal(
        observations.addOrUpdateCalls.length,
        1,
        "late rows wholly outside the newest desired range must be rejected"
    );
    assert.deepEqual(
        observations.addOrUpdateCalls[0][0].deep_unpack().slice(4, 6),
        [1500, 1600]
    );
    manager.destroy();
}
'''
new = '''function testRangeAdmissionDelegatesOpaqueRowsToNativeBoundary() {
    const { EventsManager, observations } = evaluateEventsManager();
    const manager = new EventsManager();
    manager._inited = true;
    manager.current_range_start = {
        to_unix() { return 1000; },
        add_days() { return this; },
    };
    manager.current_range_end = {
        to_unix() { return 2000; },
        add_days() {
            return { to_unix() { return 3000; } };
        },
    };
    manager._rangeState.finishRequest(manager._rangeState.beginRequest(), true);
    manager._rangeState.acceptCompletedRequest();

    const firstOpaqueRow = Object.freeze({ transportRow: "first" });
    const secondOpaqueRow = Object.freeze({ transportRow: "second" });
    manager._ingestEvents({
        unpack() {
            return [firstOpaqueRow, secondOpaqueRow];
        },
    });

    assert.equal(
        observations.addOrUpdateCalls.length,
        2,
        "JavaScript must delegate every admitted transport row to native range filtering"
    );
    assert.equal(observations.addOrUpdateCalls[0][0], firstOpaqueRow);
    assert.equal(observations.addOrUpdateCalls[1][0], secondOpaqueRow);
    for (const call of observations.addOrUpdateCalls) {
        assert.equal(call[2], 1000, "native admission receives range start");
        assert.equal(call[3], 2999, "native admission receives inclusive range end");
    }
    manager.destroy();
}
'''
assert source.count(old) == 1, "expected one obsolete JS range-schema test"
source = source.replace(old, new, 1)

old = "testRangeAdmissionRejectsLateOutOfRangeSignals();"
new = "testRangeAdmissionDelegatesOpaqueRowsToNativeBoundary();"
assert source.count(old) == 1, "expected old range-admission test invocation"
source = source.replace(old, new, 1)

path.write_text(source, encoding="utf-8")
