--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local core = require("porch.core")
local env = require("porch.env")
local tty = core.tty

local debug_categories = {
	bootstrap = true,
}
local function process_debug_env(debug_env)
	local debugtbl = {}

	debug_env = debug_env:lower()
	for cat in debug_env:gmatch("[^, ]+") do
		assert(debug_categories[cat] ~= nil,
		    "Unknown debug category: " .. cat)
		debugtbl[cat] = true
	end

	return debugtbl
end

local MatchBuffer = {}
function MatchBuffer:new(process, ctx)
	local obj = setmetatable({}, self)
	self.__index = self

	obj.buffer = ""
	obj.ctx = ctx
	obj.process = process
	obj.eof = false
	return obj
end
function MatchBuffer:_matches(action)
	local first, last, callback = action:matches(self.buffer)

	if not first then
		return false
	end

	-- On match, we need to trim the buffer and signal completion.
	action.completed = true
	self.buffer = self.buffer:sub(last + 1)

	-- Return value is not significant, ignored.
	callback = callback or action.callback
	if callback then
		self.ctx:execute(callback)
	end

	return true
end
function MatchBuffer:contents()
	return self.buffer
end
function MatchBuffer:flush(timeout)
	if not self.eof then
		self:refill(nil, timeout)
	end

	return self.eof
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

		if self.process.log then
			self.process.log:write(input)
		end

		self.buffer = self.buffer .. input
		if type(action) == "table" then
			return self:_matches(action)
		elseif action then
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

	if ctx.remote then
		-- Prefix the command with the remote configuration
		if not ctx.remote["rsh"] then
			error("rsh required for remote host spec")
		end

		local rsh_cmd = ctx.remote["rsh"]
		local full_cmd = {}
		for i = 1, #rsh_cmd do
			full_cmd[i] = rsh_cmd[i]
		end

		-- Append the host, if it's set
		if ctx.remote["host"] then
			full_cmd[#full_cmd + 1] = ctx.remote["host"]
		end

		for i = 1, #cmd do
			full_cmd[#full_cmd + 1] = cmd[i]
		end

		cmd = full_cmd
	end
	pwrap._process = assert(core.spawn(table.unpack(cmd)))
	pwrap.buffer = MatchBuffer:new(pwrap, ctx)
	pwrap.cfg = {}
	pwrap.ctx = ctx
	pwrap.is_raw = false
	pwrap.env = env:new(ctx.env)

	-- We capture PORCH_DEBUG at the time of creation in case the caller
	-- wants to debug different features at different processes.
	pwrap.debug_env = os.getenv("PORCH_DEBUG") or ""

	pwrap.term = assert(pwrap._process:term())
	local mask = pwrap.term:fetch("lflag")

	mask = mask & ~tty.lflag.ECHO
	assert(pwrap.term:update({
		lflag = mask,
	}))

	return pwrap
end
-- Proxied through to the wrapped process
function Process:chdir(dir)
	return assert(self._process:chdir(dir))
end
function Process:continue(...)
	local ret = assert(self._process:continue(...))
	self.is_stopped = false
	return ret
end
function Process:debugging(cat)
	if not self.debug then
		self.debug = process_debug_env(self.debug_env)
	end

	return self.debug[cat:lower()] ~= nil
end
function Process:eof(...)
	return self._process:eof(...)
end
function Process:gid(...)
	local args = {...}
	if #args == 0 or args[1] == nil then
		return self._process:gid()
	end

	return self:setid(nil, args[1])
end
function Process:proxy(...)
	if not self:released() then
		self:release()
	end
	return self._process:proxy(...)
end
function Process:released()
	return self._process:released()
end
function Process:release()
	local penv
	if self.env:dirty() then
		penv = self.env
	end
	return self._process:release(penv)
end
function Process:read(func, timeout)
	if timeout then
		return self._process:read(func, timeout)
	else
		return self._process:read(func)
	end
end
function Process:raw(is_raw)
	local prev_raw = self.is_raw
	self.is_raw = is_raw
	return prev_raw
end
function Process:setgroups(...)
	return assert(self._process:setgroups(...))
end
function Process:setid(uid, gid)
	return assert(self._process:setid(uid, gid))
end
local function sigapply(mask, applyval, ...)
	local signals = {...}
	local changed = false

	local function apply_signals(sigtbl)
		for _, signo in ipairs(sigtbl) do
			if type(signo) == "table" then
				apply_signals(signo)
			elseif signo > 0 and mask[signo] ~= applyval then
				changed = true
				mask[signo] = applyval
			end
		end
	end

	apply_signals(signals)
	return changed, mask
end
function Process:sigblock(...)
	local changed, newmask = sigapply(self:sigmask(), true, ...)
	if changed then
		return assert(self:sigmask(newmask))
	end

	return true
end
function Process:sigcatch(...)
	return assert(self._process:sigcatch(...))
end
function Process:sigignore(...)
	local catchmask = self:sigcatch()
	local newmask = {}

	for signo, c in ipairs(catchmask) do
		newmask[signo] = c
	end

	-- Ignoring is removing signals from the caught mask
	local changed = sigapply(newmask, false, ...)
	if changed then
		-- Calculate the delta: bits unset in the new mask that were
		-- set in the old mask are the ones that we're newly ignoring.
		for signo, c in ipairs(catchmask) do
			catchmask[signo] = c and (not newmask[signo])
		end

		return assert(self:sigcatch(false, catchmask))
	end

	return true
end
function Process:sigisblocked(...)
	local mask = self:sigmask()
	local signals = {...}

	assert(#signals > 0, "At least one signal to check is required")
	for _, signo in ipairs(signals) do
		if not mask[signo] then
			return false
		end
	end
	return true
end
function Process:sigiscaught(...)
	local catchmask = self:sigcatch()
	local signals = {...}

	assert(#signals > 0, "At least one signal to check is required")
	for _, signo in ipairs(signals) do
		if not catchmask[signo] then
			return false
		end
	end
	return true
end
function Process:sigisignored(...)
	local signals = {...}

	assert(#signals > 0, "At least one signal to check is required")
	for _, signo in ipairs(signals) do
		if self:sigiscaught(signo) then
			return false
		end
	end
	return true
end
function Process:sigisunblocked(...)
	local signals = {...}

	assert(#signals > 0, "At least one signal to check is required")
	for _, signo in ipairs(signals) do
		if self:sigisblocked(signo) then
			return false
		end
	end
	return true
end
function Process:sigreset(preserve_sigmask)
	local mask

	-- This is the only way to reset all of our caught signals, so we have
	-- a flag to skip resetting the signal mask.
	if not preserve_sigmask then
		local need_reset = false

		mask = self:sigmask()
		for _, blocked in ipairs(mask) do
			if blocked then
				need_reset = true
				break
			end
		end

		if need_reset then
			assert(self:sigmask(0))
		end
	end

	local need_sigcatch = false

	mask = self:sigcatch()
	for signo, c in ipairs(mask) do
		if not c then
			mask[signo] = true
			need_sigcatch = true
		else
			-- No change required here, unset it.
			mask[signo] = false
		end
	end

	if need_sigcatch then
		assert(self:sigcatch(true, mask))
	end

	return true
end
function Process:sigunignore(...)
	local catchmask = self:sigcatch()
	local newmask = {}

	for signo, c in ipairs(catchmask) do
		newmask[signo] = c
	end

	local changed = sigapply(newmask, true, ...)
	if changed then
		-- Calculate the delta: bits set in the new mask that were
		-- not set in the old mask.
		for signo, c in ipairs(catchmask) do
			catchmask[signo] = (not c) and newmask[signo]
		end

		return assert(self:sigcatch(true, catchmask))
	end

	return true
end
function Process:sigunblock(...)
	local changed, newmask = sigapply(self:sigmask(), false, ...)
	if changed then
		return assert(self:sigmask(newmask))
	end

	return true
end
function Process:sigmask(...)
	return assert(self._process:sigmask(...))
end
function Process:signal(signo)
	return self._process:signal(signo)
end
function Process:stop()
	local ret = assert(self._process:stop())
	self.is_stopped = true
	return ret
end
function Process:stopped()
	return self.is_stopped
end
function Process:uid(...)
	local args = {...}
	if #args == 0 or args[1] == nil then
		return self._process:uid()
	end

	return self:setid(args[1])
end
function Process:write(data, cfg)
	if not self.is_raw then
		-- Convert ^[A-Z] -> cntrl sequence
		local quoted = false
		for i = 1, #data do
			if i > #data then
				break
			end

			local ch = data:sub(i, i)

			if quoted then
				quoted = false
			elseif ch == "\\" then
				quoted = true
				data = data:sub(1, i - 1) .. data:sub(i + 1)
			elseif ch == "^" then
				if i == #data then
					error("Incomplete CNTRL character at end of buffer")
				end

				local esch = data:sub(i + 1, i + 1)
				local esc = string.byte(esch)
				if esc < 0x40 or esc > 0x5f then
					error("Invalid escape of '" .. esch .. "'")
				end

				esch = string.char(esc - 0x40)
				data = data:sub(1, i - 1) .. esch .. data:sub(i + 2)
			end
		end
	end
	if self.log then
		self.log:write(data)
	end

	local bytes, delay
	local function set_rate(which_cfg)
		if not which_cfg or not which_cfg.rate then
			return
		end

		local rate = which_cfg.rate

		if rate.bytes ~= nil then
			bytes = rate.bytes
		end
		if rate.delay ~= nil then
			delay = rate.delay
		end
	end

	-- Give process configuration a first go at it
	set_rate(self.cfg)
	set_rate(cfg)

	-- If we didn't have a configured rate, just send a single batch of all
	-- data without delay.
	if not bytes or bytes == 0 then
		bytes = #data
		delay = nil
	end

	local sent = 0
	local total = #data

	while sent < total do
		local bound = math.min(total, sent + bytes)

		assert(self._process:write(data:sub(sent + 1, bound)))
		sent = bound

		if delay and sent < total then
			core.sleep(delay)
		end
	end

	return sent
end
function Process:close()
	local function procdrain()
		-- It's imperative that we not try to drain a process that
		-- hasn't been started yet; there won't be anything pending
		-- anyways, and trying to release it may end up catching us a
		-- SIGPIPE.
		if self.buffer.eof or not self._process:released() then
			return
		end

		self.buffer:refill()
	end

	assert(self._process:close(procdrain))

	-- Flush output, close everything out
	self:logfile(nil)
	self._process = nil
	self.term = nil
	return true
end
-- Our own special salt
function Process:logfile(file)
	if self.log then
		self.log:flush()
		self.log:close()
	end

	self.log = file
end
function Process:match(action)
	local buffer = self.buffer
	if not buffer:match(action) then
		if not self.ctx:fail(action, buffer:contents()) then
			return false
		end
	end

	return true
end
function Process:set(cfg)
	for k, v in pairs(cfg) do
		self.cfg[k] = v
	end
end

return Process
