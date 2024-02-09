--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local core = require("orch.core")
local tty = core.tty

local MatchBuffer = {}
function MatchBuffer:new(process, ctx)
	local obj = setmetatable({}, self)
	self.__index = self
	self.buffer = ""
	self.ctx = ctx
	self.process = process
	self.eof = false
	return obj
end
function MatchBuffer:_matches(action)
	local first, last = action:matches(self.buffer)

	if not first then
		return false
	end

	-- On match, we need to trim the buffer and signal completion.
	action.completed = true
	self.buffer = self.buffer:sub(last + 1)

	-- Return value is not significant, ignored.
	if action.callback then
		self.ctx:execute(action.callback)
	end

	return true
end
function MatchBuffer:contents()
	return self.buffer
end
function MatchBuffer:empty()
	return #self.buffer == 0
end
function MatchBuffer:refill(action, timeout)
	assert(not self.eof)

	if not self.process:released() then
		self.process:release()
	end
	local function refill(input)
		if not input then
			self.eof = true
			return true
		end

		self.buffer = self.buffer .. input
		if type(action) == "table" then
			return self:_matches(action)
		else
			assert(type(action) == "function")

			return action()
		end
	end

	if timeout then
		assert(self.process:read(refill, timeout))
	else
		assert(self.process:read(refill))
	end
end
function MatchBuffer:match(action)
	if not self:_matches(action) and not self.eof then
		self:refill(action, action.timeout)
	end

	return action.completed
end

-- Wrap a process and perform operations on it.
local Process = {}
function Process:new(cmd, ctx)
	local pwrap = setmetatable({}, self)
	self.__index = self

	pwrap._process = assert(core.spawn(table.unpack(cmd)))
	pwrap.buffer = MatchBuffer:new(pwrap, ctx)
	pwrap.ctx = ctx

	pwrap.term = assert(pwrap._process:term())
	local mask = pwrap.term:fetch("lflag")

	mask = mask & ~tty.lflag.ECHO
	assert(pwrap.term:update({
		lflag = mask,
	}))

	return pwrap
end
-- Proxied through to the wrapped process
function Process:released()
	return self._process:released()
end
function Process:release()
	return self._process:release()
end
function Process:read(func, timeout)
	return self._process:read(func, timeout)
end
function Process:raw(text)
	return self._process:raw(text)
end
function Process:write(data)
	return self._process:write(data)
end
function Process:close()
	assert(self._process:close())

	self._process = nil
	self.term = nil
	return true
end
-- Our own special salt
function Process:match(action)
	local buffer = self.buffer
	if not buffer:match(action) then
		if not self.ctx:fail(action, buffer:contents()) then
			return false
		end
	end

	return true
end

return Process
