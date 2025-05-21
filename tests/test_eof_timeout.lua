--
-- Copyright (c) 2025 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

require('./libtest')
local porch = require('porch')
local signals = porch.signals

local function sanity_check(cmd)
	assert(cmd:match(">>"), "Failed to get a prompt")
	assert(cmd:write("Test Line\r"))
	assert(cmd:match("Test Line"), "Script does not appear functional")
end

local function check_signal(exit_status, signo)
	local exit_signal = assert(exit_status:status())

	assert(exit_status:is_signaled(), "Expected process signaled reflected in wait status")
	assert(exit_signal == signo, "Expected signal of " .. signo ..
	    ", got " .. exit_signal)
end

local echo = assert(porch.spawn("./echo_prompt.sh"))

sanity_check(echo)
assert(echo:signal(signals.SIGUSR1));

-- Default behavior: hang until process exit
local is_eof, exit_status = echo:eof()

assert(is_eof, "Process did not close stdio")
assert(exit_status, "Process failed to exit")
check_signal(exit_status, signals.SIGUSR1)

assert(echo:close())

echo = assert(porch.spawn("./echo_prompt.sh"))

sanity_check(echo)
assert(echo:signal(signals.SIGUSR1));

-- Read to eof independently of checking for process exit; the process will have
-- closed stdio immediately then slept.  This is split out so that we can read
-- to eof without timing out as soon as below.
echo:flush()

-- Timeout == 0: don't hang, just check if the process exited (it did not)
is_eof, exit_status = echo:eof(0)

assert(is_eof, "Process did not close stdio")
assert(not exit_status, "Process exited too soon")

is_eof, exit_status = echo:eof()
assert(is_eof, "eof status changed after we observed it")
assert(exit_status, "Process did not exit")
check_signal(exit_status, signals.SIGUSR1)

assert(echo:close())

echo = assert(porch.spawn("./echo_prompt.sh"))

sanity_check(echo)
assert(echo:signal(signals.SIGUSR1));

-- Timeout > 0: wait up to N seconds for the process to exit
is_eof, exit_status = echo:eof(5)

assert(is_eof, "Process did not close stdio")
assert(exit_status, "Process did not exit")
check_signal(exit_status, signals.SIGUSR1)

assert(echo:close())

echo = assert(porch.spawn("./echo_prompt.sh"))

sanity_check(echo)
assert(echo:signal(signals.SIGUSR2));

-- Timeout == 0: don't hang, just check if the process exited (it did)
porch.sleep(2)
is_eof, exit_status = echo:eof(0)

assert(is_eof, "Process did not close stdio")
assert(exit_status, "Process did not exit")
check_signal(exit_status, signals.SIGUSR2)

assert(echo:close())
