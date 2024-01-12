--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local impl = require("orch_impl")

local execute, match_ctx_stack

-- Sometimes a queue, sometimes a stack.  Oh well.
local Queue = {}
function Queue:push(item)
	self.elements[#self.elements + 1] = item
end
function Queue:back()
	return self.elements[#self.elements]
end
function Queue:remove(elem)
	for k, v in ipairs(self.elements) do
		if v == elem then
			for nk = k + 1, #self.elements do
				self.elements[nk - 1] = self.elements[nk]
			end

			self.elements[#self.elements] = nil

			return true
		end
	end
end
function Queue:pop()
	local item = self.elements[#self.elements]
	self.elements[#self.elements] = nil
	return item
end
function Queue:count()
	return #self.elements
end
function Queue:empty()
	return #self.elements == 0
end
function Queue:each(cb)
	for _, v in ipairs(self.elements) do
		cb(v)
	end
end
function Queue:items()
	return self.elements
end

local MatchAction = {}
function MatchAction:new(action, func)
	local obj = setmetatable({}, self)
	self.__index = self
	obj.type = action
	if action ~= "match" then
		obj.execute = assert(func, "Not implemented on type '" .. action .. "'")
	end
	obj.completed = false
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

local MatchBuffer = {}
function MatchBuffer:new()
	local obj = setmetatable({}, self)
	self.__index = self
	self.buffer = ""
	self.eof = false
	return obj
end
function MatchBuffer:_matches(action)
	local first, last = self.buffer:find(action.pattern)

	if not first then
		return false
	end

	-- On match, we need to trim the buffer and signal completion.
	action.completed = true
	self.buffer = self.buffer:sub(last + 1)

	-- Return value is not significant, ignored.
	if action.callback then
		execute(action.callback, true)
	end

	return true
end
function MatchBuffer:empty()
	return #self.buffer == 0
end
function MatchBuffer:refill(action, timeout)
	assert(not self.eof)

	if not impl.released() then
		impl.release()
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

	-- XXX action.timeout
	if timeout then
		impl.read(refill, timeout)
	else
		impl.read(refill)
	end
end
function MatchBuffer:match(action)
	if not self:_matches(action) and not self.eof then
		self:refill(action)
	end

	return action.completed
end

local MatchContext = setmetatable({}, { __index = Queue })
function MatchContext:new()
	local obj = setmetatable({}, self)
	self.__index = self
	obj.elements = {}
	obj.last_processed = 0
	obj.errors = false
	return obj
end
function MatchContext:dump(level)
	level = level or 1
	self:each(function(action)
		action:dump(level)
	end)
end
function MatchContext:error()
	return self.errors
end
function MatchContext:process(buffer)
	local latest = self.last_processed
	local actions = self:items()

	for idx, action in ipairs(actions) do
		if idx <= latest then
			goto skip
		end

		self.last_processed = idx
		if action.type == "match" then
			local ctx_cnt = match_ctx_stack:count()

			if not buffer:match(action) then
				-- Error out... not acceptable at all.
				self.errors = true
				return false
			end

			-- Even if this is the last element, doesn't matter; we're finished
			-- here.
			if match_ctx_stack:count() ~= ctx_cnt then
				break
			end
		elseif not action:execute(action, arg) then
			return false
		end

		::skip::
	end

	return self.last_processed == #actions
end
function MatchContext:process_one(buffer)
	local elapsed = 0

	-- Return low, high timeout of current batch
	local function get_timeout()
		return 5, 5
	end

	local actions = self:items()
	local matched

	local function match_any()
		for _, action in ipairs(actions) do
			if buffer:_matches(action) then
				matched = true
				return true
			end
		end

		return false
	end

	local tlo, thi
	local start = impl.time()

	while not matched and not buffer.eof do
		-- We recalculate every iteration to rule out any actions that have
		-- timed out.
		tlo, thi = get_timeout()

		elapsed = impl.time() - start
		if elapsed >= thi then
			break
		end

		buffer:refill(match_any, tlo)
	end

	if not matched then
		self.errors = true
		return false
	end

	return true
end

match_ctx_stack = setmetatable({ elements = {} }, { __index = Queue })
function match_ctx_stack:dump()
	self:each(function(ctx)
		ctx:dump()
	end)
end


local orch_env = {}

-- Execute a chunk; may either be a callback from a match block, or it may be
-- an entire included file.  Either way, each execution gets a new match context
-- that we may or may not use.  We'll act upon the latest in the stack no matter
-- what happens.
execute = function(func, freshctx)
	if freshctx then
			match_ctx = MatchContext:new()
	end

	assert(pcall(func))

	if freshctx then
		if not match_ctx:empty() then
			match_ctx_stack:push(match_ctx)
		else
			match_ctx = match_ctx_stack:back()
		end
	end
end

local function include_file(file)
	local f = assert(impl.open(file))
	local chunk = assert(f:read("a"))
	local func = assert(load(chunk, file, "t", orch_env))

	return execute(func, true)
end

local function do_write(action)
	assert(impl.write(action.data))
	return true
end

local function do_one(obj)
	match_ctx_stack:push(obj.match_ctx)
	return false
end

-- Setup the various bits of orch_env
function orch_env.match(pattern)
	local match_action = MatchAction:new("match")
	match_action.pattern = pattern

	local function set_cfg(cfg)
		for k, v in pairs(cfg) do
			-- XXX configurable
			if k == "pattern" or k == "type" or k == "execute" then
				error("pattern is not a valid cfg field")
			end

			match_action[k] = v
		end
	end

	match_ctx:push(match_action)
	return set_cfg
end

function orch_env.write(str)
	local action_obj = MatchAction:new("write", do_write)
	action_obj.data = str
	match_ctx:push(action_obj)
	return true
end

function orch_env.release()
	local action_obj = MatchAction:new("release", impl.release)
	match_ctx:push(action_obj)
	return true
end

function orch_env.one(func)
	local action_obj = MatchAction:new("one", do_one)
	local parent_ctx = match_ctx

	parent_ctx:push(action_obj)

	action_obj.match_ctx = MatchContext:new()
	action_obj.match_ctx.process = match_ctx.process_one

	-- Nest this one inside our action
	match_ctx = action_obj.match_ctx

	-- Now execute it
	execute(func)

	-- Sanity check the script
	for _, action in ipairs(match_ctx:items()) do
		if action.type ~= "match" then
			error("Type '" .. action.type .. "' not legal in a one() block")
		end

		-- XXX timeout should be illegal, must set before one() block.
	end

	-- Return to the parent context; that's the one we'll be acting on
	-- action.
	match_ctx = parent_ctx

	return true
end

function orch_env.debug(str)
	print(str)
end

local function run_script()
	local done

	local buffer = MatchBuffer:new()

	-- To run the script, we'll grab the back of the context stack and process
	-- that.
	while not done do
		local ctx = match_ctx_stack:back()

		if ctx:process(buffer) then
			match_ctx_stack:remove(ctx)
			done = match_ctx_stack:empty()
		elseif ctx:error() then
			return false
		end
	end

	return true
end

include_file(impl.script)
--match_ctx_stack:dump()

impl.exit(run_script())