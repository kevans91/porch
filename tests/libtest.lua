--
-- Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
--
-- SPDX-License-Identifier: BSD-2-Clause
--

local ltest = {}

local libpath = os.getenv("PORCHLIB_PATH")
if libpath and #libpath == 0 then
	libpath = nil
end

local luapath = os.getenv("PORCHLUA_PATH")
if luapath and #luapath == 0 then
	luapath = nil
end

local function porchloader(modname)
	if not modname:match("^porch") then
		return
	end

	local function loadlua(_, chunk)
		return chunk()
	end

	if modname == "porch" then
		if not luapath then
			return
		end
		return loadlua, loadfile(luapath .. "/" .. modname .. ".lua", "t")
	end

	assert(modname:match("^porch."))
	modname = modname:sub(#"porch." + 1)

	if luapath then
		local function lua_path(name)
			return luapath .. "/porch/" .. name .. ".lua"
		end
		local function try_lua(name)
			return loadfile(lua_path(name), "t")
		end

		local chunk = try_lua(modname)
		if chunk then
			return loadlua, chunk
		end
	end

	if libpath then
		-- Must be a C module
		local function clib_path(name, ext)
			return libpath .. "/" .. name .. "/" .. name .. ext
		end
		local function try_clib(ext)
			return package.loadlib(clib_path(modname, ext), "luaopen_porch_" ..
			    modname)
		end

		local openfunc = try_clib(".so") or try_clib(".dylib")
		if not openfunc then
			-- luapath is optional, so if we fail here then that could
			-- mean we're looking at a .lua file that should just fall
			-- through to the other searchers.
			return
		end

		local function loadc(_, opener)
			return opener()
		end

		return loadc, openfunc
	end
end

if libpath or luapath then
	-- We take care to insert our searcher first, otherwise the default set
	-- may find system libs before it finds our build products.
	for i = #package.searchers, 1, -1 do
		package.searchers[i + 1] = package.searchers[i]
	end
	package.searchers[1] = porchloader
end

return ltest
