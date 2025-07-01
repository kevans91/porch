#
# Copyright (c) 2024, 2025 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

file(GLOB test_files
	${CMAKE_CURRENT_SOURCE_DIR}/my_cat
	${CMAKE_CURRENT_SOURCE_DIR}/*.lua
	${CMAKE_CURRENT_SOURCE_DIR}/*.orch
	${CMAKE_CURRENT_SOURCE_DIR}/*.sh
	${CMAKE_CURRENT_SOURCE_DIR}/*.script)
add_custom_target(check-setup
	COMMAND cp ${test_files} ${CMAKE_CURRENT_BINARY_DIR})

add_executable(echo_prompt echo_prompt.c)
add_executable(openv openv.c)
add_executable(printid printid.c)
add_executable(sigcheck sigcheck.c ${CMAKE_SOURCE_DIR}/lib/core/porch_signals.c)
add_executable(stopwatch stopwatch.c)
target_include_directories(sigcheck PRIVATE ${CMAKE_SOURCE_DIR}/lib)

# check-cli and check-lib are intended to run our tests exclusively against the
# build products.
add_custom_target(check-cli
	COMMAND env PORCHBIN="${CMAKE_BINARY_DIR}/src/porch" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/share/lua" sh "${CMAKE_CURRENT_BINARY_DIR}/include_test.sh"
	COMMAND env PORCHBIN="${CMAKE_BINARY_DIR}/src/porch" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/share/lua" sh "${CMAKE_CURRENT_BINARY_DIR}/basic_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS check-setup openv porch)
add_custom_target(check-lib
	COMMAND env PORCHLIB_PATH="${CMAKE_BINARY_DIR}/lib" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/share/lua" LUA_VERSION_MAJOR="${LUA_VERSION_MAJOR}" LUA_VERSION_MINOR="${LUA_VERSION_MINOR}" sh "${CMAKE_CURRENT_BINARY_DIR}/lua_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS check-setup)

# check-install does both of the above, but only against an installed porch(1)
# to confirm that the installation is sane.
add_custom_target(check-install
	COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/basic_test.sh"
	COMMAND env LUA_VERSION_MAJOR="${LUA_VERSION_MAJOR}" LUA_VERSION_MINOR="${LUA_VERSION_MINOR}" sh "${CMAKE_CURRENT_BINARY_DIR}/lua_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS check-setup)

add_custom_target(check DEPENDS check-cli check-lib check-man)

file(GLOB lua_test_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/*.lua")

add_custom_target(lint-tests
	COMMAND echo LINTING FOR LUA 5.3
	COMMAND luacheck --std=lua53 ${lua_test_SOURCES}
	COMMAND echo LINTING FOR LUA 5.4
	COMMAND luacheck --std=lua54 ${lua_test_SOURCES}
)
