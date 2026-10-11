#!/usr/bin/env python3
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tests/test-js-runtime.js"
source = path.read_text(encoding="utf-8")
old = '''    let use24h = true;
    const view = Object.create(EventList.prototype);
    const actors = [];
    Object.assign(view, {
        _destroyed: false,
        selected_date: null,
        _rows: [],
        _current_event_cache_key: null,
        _canLaunchCalendar: false,
        desktop_settings: {
            get_boolean(key) {
                assert.equal(key, "clock-use-24h");
                return use24h;
            },
        },
'''
new = '''    const view = Object.create(EventList.prototype);
    const actors = [];
    Object.assign(view, {
        _destroyed: false,
        selected_date: null,
        _rows: [],
        _current_event_cache_key: null,
        _canLaunchCalendar: false,
        _use24h: true,
'''
assert source.count(old) == 1, "expected one agenda cache settings fixture"
source = source.replace(old, new, 1)
old = '''    const tuesdayRow = view._rows[0];
    use24h = false;
    view.set_events(snapshot("Tuesday event"), false);
'''
new = '''    const tuesdayRow = view._rows[0];
    view.set_use_24h(false);
    view.set_events(snapshot("Tuesday event"), false);
'''
assert source.count(old) == 1, "expected one agenda cache preference transition"
source = source.replace(old, new, 1)
path.write_text(source, encoding="utf-8")
