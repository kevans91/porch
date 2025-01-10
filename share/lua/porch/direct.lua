--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local actions = require('porch.actions')
local context = require('porch.context')
local matchers = require('porch.matchers')
local process = require('porch.process')

local direct = {}

direct.defaults = {
	timeout = 10,
}

local direct_ctx = context:new()
function direct_ctx.execute(_, callback)
	callback()
end
function direct_ctx:fail(_, contents)
	if self.fail_handler then
		self.fail_handler(contents)
	end
	return false
end

-- Wraps a process, provide everything we offer in actions.defined as a wrapper
local DirectProcess = {}
function DirectProcess:new(cmd, ctx)
	local pwrap = setmetatable({}, self)
	self.__index = self

	pwrap._process = process:new(cmd, ctx)
	pwrap.ctx = ctx
	pwrap.term = pwrap._process.term
	pwrap.timeout = direct.defaults.timeout

	ctx.process = pwrap._process

	return pwrap
end
function DirectProcess:close()
	return self._process:close()
end
function DirectProcess:match(pattern, matcher)
	matcher = matcher or matchers.available.default

	local action = actions.MatchAction:new("match")
	local patterns

	if type(pattern) == "table" then
		patterns = pattern
	else
		patterns = { [pattern] = {} }
	end
	action.timeout = self.timeout
	action.matcher = matcher

	if matcher.compile then
		for pat, v in pairs(patterns) do
			v._compiled = action.matcher.compile(pat)
		end
	end

	action.patterns = patterns

	return self._process:match(action)
end
function DirectProcess:proxy(...)
	return self._process:proxy(...)
end
for name, def in pairs(actions.defined) do
	-- Each of these gets a function that generates the action and then
	-- subsequently executes it.
	DirectProcess[name] = function(pwrap, ...)
		local action = actions.MatchAction:new(name, def.execute)
		local args = { ... }

		action.ctx = pwrap.ctx

		local ok, res

		if def.init then
			ok, res = pcall(def.init, action, args)
			if not ok then
				return nil, res
			end
		end

		ok, res = pcall(action.execute, action)
		if not ok then
			-- res is the error object
			return nil, res
		end

		return res
	end
end

function direct.spawn(...)
	local fresh_ctx = {}

	for k, v in pairs(direct_ctx) do
		fresh_ctx[k] = v
	end

	return DirectProcess:new({...}, fresh_ctx)
end

return direct