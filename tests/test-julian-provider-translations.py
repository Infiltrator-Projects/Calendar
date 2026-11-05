#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check translations in the independently installed Julian resource."""

import ctypes
import os
import sys


class Provider(ctypes.Structure):
    pass


Format = ctypes.CFUNCTYPE(
    ctypes.c_bool,
    ctypes.POINTER(Provider),
    ctypes.c_int64,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_char),
    ctypes.c_size_t,
    ctypes.POINTER(ctypes.c_size_t),
)
Provider._fields_ = [
    ("struct_size", ctypes.c_size_t),
    ("abi_version", ctypes.c_uint32),
    ("calendar_id", ctypes.c_char_p),
    ("fields_from_jdn", ctypes.c_void_p),
    ("format", Format),
    ("period_start", ctypes.c_void_p),
    ("add_periods", ctypes.c_void_p),
    ("add_years", ctypes.c_void_p),
]

module = ctypes.CDLL(sys.argv[1])
entry = module.infiltratr_temporal_calendar_provider_v1
entry.restype = ctypes.POINTER(Provider)
provider = entry()
assert provider and provider.contents.abi_version == 1
assert provider.contents.calendar_id == b"julian"

# JDN 2460324 is 1 January 2024 in the Julian calendar. The provider owns
# its translations; this check runs after the Calendar app has been removed.
for language, expected in (("en_US", "January"), ("fr", "Janvier")):
    os.environ["LANGUAGE"] = language
    output = ctypes.create_string_buffer(512)
    length = ctypes.c_size_t()
    assert provider.contents.format(
        provider, 2460324, 2, output, len(output), ctypes.byref(length)
    )
    assert output.value.decode("utf-8") == expected
    assert length.value == len(output.value)

print("Julian provider embedded translations remain available")
