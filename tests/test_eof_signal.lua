--
-- Copyright (c) 2026 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

require('./libtest')
local porch = require('porch')
local ok, err

ok, err = porch.run_script("test_eof_signal_crash.script")
assert(ok == nil, "eof() did not notice the process was killed")
assert(err:match("killed with signal"), "eof() did not return a proper error")

ok, err = porch.run_script("test_eof_signal_crash_callback.script")
assert(ok == nil, "eof() callback hid the process being killed")
assert(err:match("killed with signal"), "eof() did not return a proper error")

assert(porch.run_script("test_eof_signal_sent.script"))

ok, err = porch.run_script("test_eof_signal_callback.script")
assert(ok == 7, "eof() callback did not get the signal status: " ..
    tostring(err))
