--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local core = require("orch.core")

local matchers = require("orch.matchers")
local process = require("orch.process")
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
	local matcher_arg = self.pattern_obj or self.pattern

	return self.matcher.match(matcher_arg, buffer)
end

actions.MatchAction = MatchAction
actions.defined = {
	eof = {
		print_diagnostics = function(action)
			io.stderr:write(string.format("[%s]:%d: eof not observed\n",
			    action.src, action.line))
		end,
		init = function(action, args)
			action.timeout = args[1] or action.ctx.timeout
		end,
		execute = function(action)
			local ctx = action.ctx
			local buffer = ctx.process.buffer

			if buffer.eof then
				return true
			end

			local function discard()
			end

			buffer:refill(discard, action.timeout)
			if not buffer.eof then
				if not ctx:fail(action, buffer:contents()) then
					return false
				end
			end

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
	sleep = {
		allow_direct = true,
		init = function(action, args)
			action.duration = args[1]
		end,
		execute = function(action)
			assert(core.sleep(action.duration))
			return true
		end,
	},
	spawn = {
		init = function(action, args)
			action.cmd = args

			if type(action.cmd[1]) == "table" then
				if #action.cmd > 1 then
					error("spawn: bad mix of table and additional arguments")
				end
				action.cmd = table.unpack(action.cmd)
			end
		end,
		execute = function(action)
			local current_process = action.ctx.process
			if current_process then
				assert(current_process:close())
			end

			action.ctx.process = process:new(action.cmd, action.ctx)
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
		end,
		execute = function(action)
			local current_process = action.ctx.process
			if not current_process then
				error("Script did not spawn process prior to writing")
			end

			assert(current_process:write(action.value))
			return true
		end,
	},
}

return actions
