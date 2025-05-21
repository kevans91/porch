--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local core = require("porch.core")

local matchers = require("porch.matchers")
local tty = core.tty

local actions = {}

actions.default_matcher = matchers.available.default
actions.default_timeout = 10

local MatchAction = {}
function MatchAction:new(action, func)
	local obj = setmetatable({}, self)
	self.__index = self
	obj.type = action
	if action ~= "match" then
		obj.execute = assert(func, "Not implemented on type '" .. action .. "'")
	end
	obj.completed = false
	obj.matcher = actions.default_matcher
	return obj
end
function MatchAction:dump(level)
	local indent = " "
	local is_one = self.type == "one"

	print(indent:rep((level - 1) * 2) .. "MATCH OBJECT [" .. self.type .. "]:")
	for k, v in pairs(self) do
		if k == "type" or (is_one and k == "match_ctx") then
			goto continue
		end

		print(indent:rep(level * 2) .. k, v)
		::continue::
	end

	if is_one and self.match_ctx then
		self.match_ctx:dump(level + 1)
	end
end
function MatchAction:matches(buffer)
	local first, last, cb
	local len

	for pattern, def in pairs(self.patterns) do
		local matcher_arg = def._compiled or pattern

		-- We use the earliest and longest match, rather than the first to
		-- match, to provide predictable semantics.  If any more control than
		-- that is desired, it should be split up into multiple distinct matches
		-- or, in a scripter context, switched to a one() block.
		local tfirst, tlast = self.matcher.match(matcher_arg, buffer)
		if not tfirst then
			goto next
		end

		if len and tfirst > first then
			-- Earlier matches take precedence.
			goto next
		end

		local tlen = tlast - tfirst
		if tfirst == first and tlen <= len then
			-- Longer matches take precedence.  If we have two
			-- patterns that managed to result in the same match,
			-- then we arbitrarily choose the first one we noticed.
			goto next
		end

		first, last, cb = tfirst, tlast, def.callback
		len = tlen
::next::
	end

	return first, last, cb
end

actions.MatchAction = MatchAction
actions.defined = {
	cfg = {
		init = function(action, args)
			action.cfg = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			if not current_process then
				error("cfg() called before process spawned.")
			end

			current_process:set(action.cfg)
			return true
		end,
	},
	chdir = {
		init = function(action, args)
			action.dir = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			if not current_process then
				error("chdir() called before process spawned.")
			end

			current_process:chdir(action.dir)
			return true
		end,
	},
	eof = {
		print_diagnostics = function(action)
			io.stderr:write(string.format("[%s]:%d: eof not observed\n",
			    action.src, action.line))
		end,
		init = function(action, args)
			action.timeout = args[1] or action.ctx.timeout
			action.callback = args[2]
		end,
		execute = function(action)
			local ctx = action.ctx
			local current_process = ctx.process
			local buffer = current_process.buffer

			if not buffer:flush(action.timeout) then
				if not ctx:fail(action, buffer:contents()) then
					return false
				end
			end

			if action.callback then
				local status = assert(current_process:eof())
				action.callback(status)
			end

			return true
		end,
	},
	flush = {
		init = function(action, args)
			action.timeout = args[1] or action.ctx.timeout
		end,
		execute = function(action)
			local current_process = action.ctx.process
			local buffer = current_process.buffer

			buffer:flush(action.timeout)
			return true
		end,
	},
	log = {
		init = function(action, args)
			local file = args[1]
			if type(file) == "string" then
				file = io.open(file, "a+")
			end

			action.file = file
		end,
		execute = function(action)
			local current_process = action.ctx.process

			if not current_process then
				error("execute() called before process spawned.")
			end

			current_process:logfile(action.file)
			return true
		end,
	},
	pipe = {
		init = function(action, args)
			action.command = args[1]
			action.linefilter = args[2]
		end,
		execute = function(action)
			local current_process = action.ctx.process
			if not current_process then
				error("Script did not spawn process prior to writing")
			end

			local inpipe = io.popen(action.command)
			if not inpipe then
				error("Failed to popen command '" .. action.command .. "'")
			end

			while true do
				local line = inpipe:read("L")
				if not line then
					break
				end

				if action.linefilter then
					line = action.linefilter(line)
				end
				if line then
					current_process:write(line)
				end
			end

			inpipe:close()
			return true
		end,
	},
	raw = {
		init = function(action, args)
			action.value = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			if not current_process then
				error("raw() called before process spawned.")
			end

			current_process:raw(action.value)
			return true
		end,
	},
	release = {
		execute = function(action)
			local current_process = action.ctx.process
			if not current_process then
				error("release() called before process spawned.")
			end

			assert(current_process:release())
			return true
		end,
	},
	signal = {
		init = function(action, args)
			action.signo = args[1]
		end,
		execute = function(action)
			local signo = action.signo
			local current_process = action.ctx.process

			if not current_process then
				error("signal() called before process spawned.")
			elseif not current_process:released() then
				error("signal() called before process release")
			end

			current_process:signal(signo)
			return true
		end,
	},
	stty = {
		init = function(action, args)
			local field = args[1]
			if not tty[field] then
				error("stty: not a valid field to set: " .. field)
			end

			action.field = field
			action.set = args[2]
			action.unset = args[3]
		end,
		execute = function(action)
			local field = action.field
			local set, unset = action.set, action.unset
			local current_process = action.ctx.process

			local value = current_process.term:fetch(field)
			if type(value) == "table" then
				set = set or {}

				-- cc
				for k, v in pairs(set) do
					value[k] = v
				end
			else
				set = set or 0
				unset = unset or 0

				-- *flag mask
				value = (value | set) & ~unset
			end

			assert(current_process.term:update({
				[field] = value
			}))

			return true
		end,
	},
	write = {
		init = function(action, args)
			action.value = args[1]
			action.cfg = args[2]
		end,
		execute = function(action)
			local current_process = action.ctx.process
			if not current_process then
				error("Script did not spawn process prior to writing")
			end

			assert(current_process:write(action.value, action.cfg))
			return true
		end,
	},
	-- Environment handling
	clearenv = {
		-- clearenv([local])
		allow_direct = true,
		init = function(action, args)
			action.global = not args[1]
		end,
		execute = function(action)
			local env
			local current_process = action.ctx.process

			if action.global then
				env = action.ctx.env
			elseif current_process then
				if current_process:released() then
					error("clearenv() called after process release")
				end
				env = current_process.env
			else
				error("clearenv(true) called before process spawned.")
			end

			env:clear()
			return true
		end,
	},
	getenv = {
		-- getenv(k)
		only_direct = true,
		init = function(action, args)
			action.key = args[1]
		end,
		execute = function(action)
			local env
			local current_process = action.ctx.process

			if current_process then
				env = current_process.env
			else
				env = action.ctx.env
			end

			return env[action.key]
		end,
	},
	setenv = {
		-- setenv(k, v[, local]) or setenv(kvtable[, local])
		allow_direct = true,
		init = function(action, args)
			local k, v = args[1], args[2]

			if type(k) == "table" then
				action.env = k
				action.global = not v
			else
				action.env = { [k] = v }
				action.global = not args[3]
			end

		end,
		execute = function(action)
			local env
			local current_process = action.ctx.process

			if action.global then
				env = action.ctx.env
			elseif current_process then
				if current_process:released() then
					error("setenv() called after process release")
				end
				env = current_process.env
			else
				error("setenv(..., true) called before process spawned.")
			end

			for k, v in pairs(action.env) do
				env[k] = v
			end
			return true
		end,
	},
}

return actions
