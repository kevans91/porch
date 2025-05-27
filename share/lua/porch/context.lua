--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local env = require('porch.env')

local Context = {}
function Context:new(def)
	local ctx = setmetatable(def or {}, self)
	self.__index = self

	-- The context gets an environment that inherits only from the caller's
	-- environment.  We could just change the caller's environment, but
	-- that doesn't work well in the direct-drive mode and we'd still have
	-- to track changes if one wants to change the global environment after
	-- spawn().  We want users to be able to set the environment all the way
	-- up to release() time to be most friendly.
	ctx.env = env:new()

	return ctx
end
function Context.execute(_)
	return true
end
function Context.fail(_)
	return false
end

return Context
