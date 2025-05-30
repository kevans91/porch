--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local core = require("porch.core")
local direct = require("porch.direct")
local generator = require("porch.generator")
local matchers = require("porch.matchers")
local scripter = require("porch.scripter")
local porch = {}

-- env: table of values that will be exposed to the porch script's environment.
-- A user of this library may add to the porch.env before calling
-- porch.run_script() and see their changes in the script's environment.
porch.env = scripter.env

-- Matchers available to the direct user.  The currently implemented matchers
-- available in matchers.available[] are: lua (default), plain, posix.
porch.matchers = matchers

-- generate_script(scriptfile, config): run the command described by the config
-- and record an .orch script from the result.
porch.generate_script = generator.generate_script

-- run_script(scriptfile[, config]): run `scriptfile` as a .orch script, with
-- an optional configuration table that may be supplied.
--
-- The currently recognized configuration items are `alter_path` (boolean) that
-- indicates that the script's directory should be added to PATH, and `command`
-- (table) to indicate the argv of a process to spawn before running the script.
porch.run_script = scripter.run_script

-- signals: table of signal names, always with a SIG prefix.
porch.signals = core.signals

-- sleep(duration): sleep for the given duration, in seconds.  Fractional
-- seconds are supported; core uses nanosleep(2) to implement sleep(), so this
-- is at least somewhat high resolution.
porch.sleep = core.sleep

-- spawn(cmd...): spawn the given command, returning a process that may be
-- manipulated as needed.  This is the primary interface we expose to users of
-- the lib; most of the things one would do with the scripting interface are
-- broken out via methods on the DirectProcess instead to provide a more
-- object-oriented feel.
porch.spawn = direct.spawn

-- Expose the tty module so that direct scripts can access, e.g., defined lflags.
porch.tty = core.tty

-- Reset all of the state; this largely means resetting the scripting bits, as
-- a user of this lib won't really need to reset anything.
function porch.reset()
	scripter.reset()
	assert(core.reset())
end

return porch
