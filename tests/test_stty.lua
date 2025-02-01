--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local ltest = require('./libtest')
local porch = require('porch')

local cat = assert(porch.spawn("cat"))
cat.timeout = 3

-- Disable ICANON and see if we get a timeout without sending VEOF.
local mask = cat.term:fetch("lflag")
mask = mask & ~porch.tty.lflag.ICANON
cat.term:update({
	lflag = mask,
})

assert(cat:write("Hello the"))
assert(cat:match("^Hello the$"), "Canonicalization appears to be enabled")
assert(cat:close())

-- Test the cc change interface, too.  Fresh cat(1) with ICANON still enabled.
cat = assert(porch.spawn("cat"))
cat.term:update({
	cc = {
		VEOF = "^F"
	}
})

assert(cat:write("Hello the^F"))
assert(cat:match("^Hello the"), "VEOF change did not take effect")
assert(cat:close())
