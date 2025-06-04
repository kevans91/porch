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

-- Just test basic ignore/block functionality
echo = assert(porch.spawn("./echo_prompt"))
echo:sigreset()

local function checkblock(process, sense, sigchk, msg)
	if type(sigchk) ~= "table" then
		sigchk = { sigchk }
	end
	if sense then
		assert(process:sigisblocked(table.unpack(sigchk)), msg)
		assert(not process:sigisunblocked(sigchk[1]),
		    "sigisunblocked does not agree with sigisblocked")
	else
		assert(not process:sigisblocked(table.unpack(sigchk)), msg)
		if #sigchk == 1 then
			assert(process:sigisunblocked(sigchk[1]),
			    "sigisunblocked does not agree with sigisblocked")
		else
			assert(process:sigisblocked(sigchk[1]),
			    "expected the first signal to be blocked")
			assert(process:sigisunblocked(sigchk[2]),
			    "expected the second signal to be unblocked")
		end
	end
end
local function checkignore(process, sense, sigchk, msg)
	if type(sigchk) ~= "table" then
		sigchk = { sigchk }
	end
	if sense then
		assert(process:sigisignored(table.unpack(sigchk)), msg)
		assert(not process:sigiscaught(sigchk[1]),
		    "sigiscaught does not agree with sigisignored")
	else
		assert(not process:sigisignored(table.unpack(sigchk)), msg)
		if #sigchk == 1 then
			assert(process:sigiscaught(sigchk[1]),
			    "sigiscaught does not agree with sigisignored")
		else
			assert(process:sigisignored(sigchk[1]),
			    "expected the first signal to be ignored")
			assert(process:sigiscaught(sigchk[2]),
			    "expected the second signal to be caught")
		end
	end
end

checkignore(echo, false, signals.SIGINT, "SIGINT is ignored despite reset")
checkblock(echo, false, signals.SIGINT, "SIGINT is blocked despite reset")

echo:sigblock(signals.SIGINT)
checkignore(echo, false, signals.SIGINT, "SIGINT is ignored after blocking")
checkblock(echo, true, signals.SIGINT, "SIGINT is not blocked despite blocking")

echo:sigignore(signals.SIGINT)
checkignore(echo, true, signals.SIGINT, "SIGINT is not ignored after ignoring")
checkblock(echo, true, signals.SIGINT, "SIGINT is suddenly unblocked after ignoring")

echo:sigreset(true)
checkignore(echo, false, signals.SIGINT, "SIGINT is ignored despite reset")
checkblock(echo, true, signals.SIGINT, "SIGINT is blocked despite reset(true)")
echo:sigreset()
checkblock(echo, false, signals.SIGINT, "SIGINT is still blocked despite reset()")

echo:sigblock(signals.SIGINT)
echo:sigignore(signals.SIGINT)
checkignore(echo, true, signals.SIGINT, "SIGINT is not ignored after ignoring")
checkignore(echo, false, {signals.SIGINT, signals.SIGHUP},
    "sigisignored() should not return true unless all signals are ignored")
checkblock(echo, true, signals.SIGINT, "SIGINT is not blocked after blocking")
checkblock(echo, false, {signals.SIGINT, signals.SIGHUP},
    "sigisblocked() should not return true unless all signals are blocked")
echo:sigblock(signals.SIGHUP)
echo:sigignore(signals.SIGHUP)
checkignore(echo, true, {signals.SIGINT, signals.SIGHUP},
    "sigisignored() should return true when all signals are ignored")
checkblock(echo, true, {signals.SIGINT, signals.SIGHUP},
    "sigisblocked() should return true when all signals are blocked")

-- We try to kill with SIGINT in close(), so let's be nice.
echo:sigunblock(signals.SIGINT)
echo:sigcatch(signals.SIGINT)
checkignore(echo, true, {signals.SIGHUP},
    "SIGHUP should not have become unignored")
checkblock(echo, true, {signals.SIGHUP},
    "SIGHUP should not have become unblocked")

-- Test sigcatch()'s other name, to be sure.
echo:sigunignore(signals.SIGHUP)
checkignore(echo, false, {signals.SIGHUP},
    "SIGHUP should be caught now")
echo:close()
