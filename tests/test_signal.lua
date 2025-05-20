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

local invoked = 0
assert(echo:eof(nil, function(status)
	local exit_code = assert(status:status())

	assert(status:is_exited(), "Expected process exit reflected in wait status")
	assert(exit_code == 37, "Expected exit status of 37, got " .. exit_code)
	invoked = invoked + 1
end))

assert(echo:close())
assert(invoked == 1,
    "Expected eof() callback to be invoked once here, not " .. invoked .. " time(s)")

local echo = assert(porch.spawn("./echo_prompt.sh"))

assert(echo:match(">>"), "Failed to get a prompt")
assert(echo:write("Test Line\r"))
assert(echo:match("Test Line"), "Script does not appear functional")
assert(echo:signal(signals.SIGKILL))

assert(echo:eof(nil, function(status)
	local signo = assert(status:status())

	assert(status:is_signaled(), "Expected process signal reflected in wait status")
	assert(signo == signals.SIGKILL,
	    "Expected exit status of " .. signals.SIGKILL .. ", got " .. signo)
	invoked = invoked + 1
end))

assert(echo:close())
assert(invoked == 2,
    "Expected eof() callback to be invoked twice here, not " .. invoked .. " time(s)")
