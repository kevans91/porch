cmake_minimum_required(VERSION 3.18)

project(orch)

find_package("Lua")
# XXX Make sure it was found?

set(LUA_MODLIBDIR "/usr/local/lib/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
	CACHE PATH "Path to install lua dynamic library modules into")
set(LUA_MODSHAREDIR "/usr/local/share/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
	CACHE PATH "Path to install lua modules into")
set(ORCHLUA_PATH "${LUA_MODLIBDIR}"
	CACHE PATH "Path to install orch.lua into")
set(ORCHLUA_BINDIR "/usr/bin"
	CACHE PATH "Path to install orch(1) into")

add_subdirectory(lib)
add_subdirectory(man)
add_subdirectory(src)
add_subdirectory(tests)
