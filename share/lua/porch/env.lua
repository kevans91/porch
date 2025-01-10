--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local ProcessEnv = {}
function ProcessEnv:new(parent)
	local pwrap = {}

	-- We have to explicitly transfer the methods we want to retain since
	-- we're doing a custom __index to pass-through to our storage.
	pwrap.clear = self.clear
	pwrap.dirty = self.dirty
	pwrap.expand = self.expand

	pwrap._parent = parent
	pwrap._clear = false
	pwrap._dirty = false
	pwrap._setenv = {}
	pwrap._unsetenv = {}

	return setmetatable(pwrap, self)
end
function ProcessEnv:__index(key)
	if self._setenv[key] then
		return self._setenv[key]
	elseif self._unsetenv[key] then
		return nil
	end

	-- Look it up in the parent if we don't a value for it and it's not
	-- unset.
	if rawget(self, "_parent") then
		return self._parent[key]
	elseif not self._clear then
		-- We didn't have a parent, so we should look it up in the
		-- caller's environment

		return os.getenv(key)
	end

	return nil
end
function ProcessEnv:__newindex(key, value)
	if not value then
		-- Clear it, make sure that we whiteout to avoid pulling it from
		-- a parent environment.
		self._setenv[key] = nil
		self._unsetenv[key] = true
		self._dirty = true
		return
	end

	self._setenv[key] = value
	self._unsetenv[key] = nil
	self._dirty = true
end
function ProcessEnv:clear()
	-- Disinherit everything from the parent / global environment
	rawset(self, "_clear", true)
	rawset(self, "_dirty",  true)
	rawset(self, "_parent",  nil)

	-- Clear out any state that we had; we start fresh.
	rawset(self, "_setenv", {})
	rawset(self, "_unsetenv", {})
end
function ProcessEnv:dirty()
	return (self._parent and self._parent._dirty) or self._dirty
end
function ProcessEnv:expand(tblres)
	local settbl, unsettbl
	local do_clear

	if self._parent then
		settbl, unsettbl, do_clear = self._parent:expand(true)
	else
		settbl = {}
	end

	do_clear = do_clear or self._clear
	if do_clear then
		-- If we're clearing the global env, then we don't need to waste
		-- space on vars to unset.  We apply unset to anything from the
		-- parent in the flattened table.
		unsettbl = nil
	else
		unsettbl = unsettbl or {}
	end
	for k in pairs(self._unsetenv) do
		settbl[k] = nil
		if unsettbl then
			unsettbl[k] = true
		end
	end
	for k, v in pairs(self._setenv) do
		settbl[k] = v
	end

	-- The above de-duplicated keys and ensured that values in the process
	-- take precedence over the global, now let's be nice and break them
	-- down into NUL-terminated fields for the C layer to just pass along.
	local function nuljoin(tbl)
		if not tbl then
			return nil
		end

		local str = ""
		for k, v in pairs(tbl) do
			if type(v) == "boolean" then
				str = str .. k .. "\0"
			else
				str = str .. k .. "=" .. v .. "\0"
			end
		end

		-- The C layer will pass the full length along, so we don't
		-- bother adding an extra NUL terminator to signal the end of
		-- the list.
		return str
	end

	-- Expand returns the environment to use, any variables to whiteout, and
	-- whether or not we cleared out any previous environment.
	if tblres then
		return settbl, unsettbl, do_clear
	else
		return nuljoin(settbl), nuljoin(unsettbl), do_clear
	end
end

return ProcessEnv
