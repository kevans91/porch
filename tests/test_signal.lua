--
-- Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

require('./libtest')
local porch = require('porch')
local signals = porch.signals

local echo = assert(porch.spawn("./echo_prompt"))

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

local _, exit_status = echo:eof()
assert(exit_status, "Process failed to exit")
local exit_code = assert(exit_status:status())

assert(exit_status:is_exited(), "Expected process exit reflected in wait status")
assert(exit_code == 37, "Expected exit status of 37, got " .. exit_code)

assert(echo:close())

echo = assert(porch.spawn("./echo_prompt"))

assert(echo:match(">>"), "Failed to get a prompt")
assert(echo:write("Test Line\r"))
assert(echo:match("Test Line"), "Script does not appear functional")
assert(echo:signal(signals.SIGKILL))

_, exit_status = echo:eof()
assert(exit_status, "Process failed to exit")
local signo = assert(exit_status:status())

assert(exit_status:is_signaled(), "Expected process signal reflected in wait status")
assert(signo == signals.SIGKILL,
    "Expected exit status of " .. signals.SIGKILL .. ", got " .. signo)

assert(echo:close())
