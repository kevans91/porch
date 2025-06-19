--
-- Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

require('./libtest')
local porch = require('porch')

local stopwatch = assert(porch.spawn("./stopwatch", "2"))
stopwatch.timeout = 3

assert(stopwatch:match("Timer start"), "stopwatch failed to start")
assert(stopwatch:stop())
porch.sleep(2)
assert(stopwatch:continue())
assert(stopwatch:match("Timer finished"), "stopwatch failed to stop")
assert(stopwatch:close())
