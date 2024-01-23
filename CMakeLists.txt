cmake_minimum_required(VERSION 3.18)

project(orch)

find_package("Lua")

if(NOT LUA_FOUND)
	message(FATAL_ERROR "No Lua installation found; please install Lua")
endif()

set(LUA_MODLIBDIR "/usr/local/lib/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
	CACHE PATH "Path to install lua dynamic library modules into")
set(LUA_MODSHAREDIR "/usr/local/share/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
	CACHE PATH "Path to install lua modules into")
set(ORCHLUA_PATH "${LUA_MODSHAREDIR}"
	CACHE PATH "Path to install orch.lua into")
set(ORCHLUA_BINDIR "/usr/bin"
	CACHE PATH "Path to install orch(1) into")

add_subdirectory(lib)
add_subdirectory(man)
add_subdirectory(src)
add_subdirectory(tests)
