--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

require('./libtest')
local porch = require('porch')

local cat = assert(porch.spawn("cat"))
cat.timeout = 3

local ok, err = cat:stty("bogus-field")
assert(ok == nil, "Action did not flag the error return properly")
assert(err:match("stty:"), "Action did not return a proper error")
