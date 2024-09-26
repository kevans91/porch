local ltest = {}

local libpath = os.getenv("PORCHLIB_PATH")
local luapath = os.getenv("PORCHLUA_PATH")

local function porchloader(modname)
	if not modname:match("^porch") then
		return nil
	end

	local function loadlua(modname, chunk)
		return chunk()
	end

	if modname == "porch" then
		return loadlua, loadfile(luapath .. "/" .. modname .. ".lua", "t")
	end

	assert(modname:match("^porch."))
	modname = modname:sub(#"porch." + 1)

	local function lua_path(name)
		return luapath .. "/porch/" .. name .. ".lua"
	end
	local function clib_path(name, ext)
		return libpath .. "/" .. name .. "/" .. name .. ext
	end
	local function try_lua(name)
		return loadfile(lua_path(name), "t")
	end

	local chunk = try_lua(modname)
	if chunk then
		return loadlua, chunk
	end

	-- Must be a C module
	local function try_clib(name, ext)
		return package.loadlib(clib_path(modname, ext), "luaopen_porch_" ..
	    modname)
	end

	local openfunc = try_clib(modname, ".so") or try_clib(modname, ".dylib")
	assert(openfunc, "Failed to load " .. modname .. " as either .lua or shlib")

	local function loadc(modname, openfunc)
		return openfunc()
	end

	return loadc, openfunc
end

-- We take care to insert our searcher first, otherwise the default set may find
-- system libs before it finds our build products.
for i = #package.searchers, 1, -1 do
	package.searchers[i + 1] = package.searchers[i]
end
package.searchers[1] = porchloader

return ltest
