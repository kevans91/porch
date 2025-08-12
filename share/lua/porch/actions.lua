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
		need_process = true,
		init = function(action, args)
			action.cfg = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			current_process:set(action.cfg)
			return true
		end,
	},
	chdir = {
		need_process = true,
		init = function(action, args)
			action.dir = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			current_process:chdir(action.dir)
			return true
		end,
	},
	continue = {
		need_process = true,
		init = function(action, args)
			action.sendsig = (#args == 0 and true) or args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			if not current_process:stopped() then
				error("stop() called on a running process")
			end

			current_process:continue(action.sendsig)
			return true
		end,
	},
	eof = {
		need_process = true,
		diagnostics = function(action)
			return string.format("[%s]:%d: eof not observed\n",
			    action.src, action.line)
		end,
		init = function(action, args)
			action.timeout = args[1] or action.ctx.timeout
			action.callback = args[2]
		end,
		execute = function(action)
			local ctx = action.ctx
			local current_process = ctx.process
			local buffer = current_process.buffer
			local failed = false

			if not buffer:flush(action.timeout) then
				if not ctx:fail(action, buffer:contents()) then
					return false
				end

				failed = true
			end

			if not failed and action.callback then
				local is_eof, status = current_process:eof(action.timeout)
				assert(is_eof)

				if status then
					action.callback(status)
				end
			end

			return true
		end,
	},
	flush = {
		need_process = true,
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
		need_process = true,
		init = function(action, args)
			local file = args[1]
			if type(file) == "string" then
				file = io.open(file, "a+")
			end

			action.file = file
			if args[2] ~= nil then
				action.log_writes = args[2]
			else
				action.log_writes = true
			end
		end,
		execute = function(action)
			local current_process = action.ctx.process

			current_process:logfile(action.file, action.log_writes)
			return true
		end,
	},
	pipe = {
		need_process = true,
		init = function(action, args)
			action.command = args[1]
			action.linefilter = args[2]
		end,
		execute = function(action)
			local current_process = action.ctx.process

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
		need_process = true,
		init = function(action, args)
			action.value = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			current_process:raw(action.value)
			return true
		end,
	},
	release = {
		need_process = true,
		execute = function(action)
			local current_process = action.ctx.process

			assert(current_process:release())
			return true
		end,
	},
	setgroups = {
		allow_direct = true,
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.setgroups = args
		end,
		execute = function(action)
			local current_process = action.ctx.process

			current_process:setgroups(table.unpack(action.setgroups))
			return true
		end,
	},
	setid = {
		allow_direct = true,
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.setuid = args[1]
			action.setgid = args[2]
		end,
		execute = function(action)
			local current_process = action.ctx.process

			current_process:setid(action.setuid, action.setgid)
			return true
		end,
	},
	sigblock = {
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.sigtbl = args
		end,
		execute = function(action)
			local current_process = action.ctx.process

			assert(current_process:sigblock(action.sigtbl))
			return true
		end,
	},
	sigclear = {
		need_process = true,
		need_prerelease = true,
		execute = function(action)
			local current_process = action.ctx.process

			assert(current_process:sigmask(0))
			return true
		end,
	},
	sigignore = {
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.sigtbl = args
		end,
		execute = function(action)
			local current_process = action.ctx.process

			assert(current_process:sigignore(action.sigtbl))
			return true
		end,
	},
	signal = {
		need_process = true,
		init = function(action, args)
			action.signo = args[1]
		end,
		execute = function(action)
			local signo = action.signo
			local current_process = action.ctx.process

			if not current_process:released() then
				error("signal() called before process release")
			end

			current_process:signal(signo)
			return true
		end,
	},
	sigreset = {
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.preserve_sigmask = args[1]
		end,
		execute = function(action)
			local current_process = action.ctx.process
			local preserve_sigmask = action.preserve_sigmask

			assert(current_process:sigreset(preserve_sigmask))
			return true
		end,
	},
	sigunblock = {
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.sigtbl = args
		end,
		execute = function(action)
			local current_process = action.ctx.process

			assert(current_process:sigunblock(action.sigtbl))
			return true
		end,
	},
	sigcatch = {
		need_process = true,
		need_prerelease = true,
		init = function(action, args)
			action.sigtbl = args
		end,
		execute = function(action)
			local current_process = action.ctx.process

			assert(current_process:sigunignore(action.sigtbl))
			return true
		end,
	},
	stop = {
		need_process = true,
		execute = function(action)
			local current_process = action.ctx.process

			-- We don't want to allow pre-release stop() unless it's
			-- already known that they're trying to debug porch(1)
			-- process bootstrap.  This is just a simple
			-- anti-footgun.
			if not current_process:released() and
			    not current_process:debugging("bootstrap") then
				error("stop() called before process release")
			elseif current_process:stopped() then
				error("stop() called on an already-stopped process")
			end

			current_process:stop()
			return true
		end,
	},
	stty = {
		need_process = true,
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
		need_process = true,
		init = function(action, args)
			action.value = args[1]
			action.cfg = args[2]
		end,
		execute = function(action)
			local current_process = action.ctx.process

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

-- Aliases
actions.defined['sigunignore'] = actions.defined['sigcatch']

return actions
