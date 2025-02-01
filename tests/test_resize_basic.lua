--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local ltest = require('./libtest')
local porch = require('porch')

local win = assert(porch.spawn("./resized.sh"))
win.timeout = 3

-- Wait for release from the resize script; signal handler setup now.
assert(win:match("ready"), "resized.sh did not become ready")

local function assert_size(aw, ah, ew, eh)
	assert(aw == ew, "width is wrong, expected " .. ew .. " got " .. aw)
	assert(ah == eh, "height is wrong, expected " .. aw .. " got " .. ah)
end

local w, h = assert(win.term:size())
assert_size(w, h, 0, 0)

w, h = win.term:size(25, 80)
assert_size(w, h, 25, 80)

w, h = win.term:size()
assert_size(w, h, 25, 80)
w, h = win.term:size()
assert_size(w, h, 25, 80)

assert(win:match("resized"), "resized.sh did not catch a SIGWINCH")
win:write("^C")

assert(win:close())
