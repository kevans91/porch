#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.18)

project(porch VERSION 1.1)

find_package("Lua")

if(NOT LUA_FOUND)
	message(FATAL_ERROR "No Lua installation found; please install Lua")
endif()

set(LUA_MODLIBDIR "lib/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
	CACHE PATH "Path to install lua dynamic library modules into")
set(LUA_MODSHAREDIR "share/lua/${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}"
	CACHE PATH "Path to install lua modules into")
if(IS_ABSOLUTE "${LUA_MODSHAREDIR}")
	set(PORCHLUA_PATH "${LUA_MODSHAREDIR}"
		CACHE PATH "Path to install porch.lua into")
else()
	set(PORCHLUA_PATH "${CMAKE_INSTALL_PREFIX}/${LUA_MODSHAREDIR}"
		CACHE PATH "Path to install porch.lua into")
endif()
set(PORCHLUA_BINDIR "bin"
	CACHE PATH "Path to install porch(1) into")
set(PORCHLUA_EXAMPLESDIR "share/examples/${CMAKE_PROJECT_NAME}"
	CACHE PATH "Path to install .orch examples into")

option(BUILD_DRIVER "Build the porch(1) driver" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_compile_definitions(_FORTIFY_SOURCE=2)
	add_compile_options(-Wno-macro-redefined)

	add_compile_options(-fsanitize=address,undefined -fstrict-aliasing)
	add_compile_options(-Wall -Wextra -Werror)
	add_link_options(-fsanitize=address,undefined -fstrict-aliasing)
endif()

add_subdirectory(lib)
add_subdirectory(share)

if(BUILD_DRIVER)
	add_subdirectory(src)
endif()
add_subdirectory(tests)

add_custom_target(lint)
add_dependencies(lint lint-share lint-tests)
