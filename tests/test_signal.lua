--
-- Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local ltest = require('./libtest')
local porch = require('porch')
local signals = porch.signals

local echo = assert(porch.spawn("./echo_prompt.sh"))

assert(echo:match(">>"), "Failed to get a prompt")
assert(echo:write("Test Line\r"))
assert(echo:match("Test Line"), "Script does not appear functional")

assert(echo:match(">>"), "Expected a re-prompt")
assert(echo:signal(signals.SIGINT));
assert(echo:match("Interrupt"), "First signal did not sent")

assert(echo:write("Test Line\r"))
assert(echo:match("Test"), "Prompt echo expected")

assert(echo:match(">>"), "Expected a re-prompt")
assert(echo:signal(signals.SIGINT));
assert(echo:match("Interrupt"), "Second signal did not send")

assert(echo:write("Test Line\r"))
assert(echo:match("Test"), "Prompt echo expected")

assert(echo:match(">>"), "Expected a re-prompt")
assert(echo:signal(signals.SIGINT));
assert(echo:match("Interrupt"), "Third signal did not send")

assert(echo:write("Test Line\r"))

assert(echo:eof())
assert(echo:close())
