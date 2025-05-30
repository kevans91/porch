--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

require('./libtest')
local porch = require('porch')

local cat = assert(porch.spawn("cat"))
cat.timeout = 3

assert(cat:write("Hello, world\r"))

assert(cat:match("Hello"), "Failed to find 'Hello'")
assert(cat:match(", "), "Failed to find ', '")

assert(cat:close())

-- Buffer should not have been dumped after close(), which gives us a chance to
-- inspect any last-minute output.
assert(cat:match("world"), "Buffer was cleared at close")
