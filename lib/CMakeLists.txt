add_subdirectory(core)
add_subdirectory(porch)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/porch.lua"
	DESTINATION "${LUA_MODSHAREDIR}")
file(GLOB lua_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/*.lua"
	"${CMAKE_CURRENT_SOURCE_DIR}/porch/*.lua")

add_custom_target(libs
	DEPENDS core)

add_custom_target(lint
	COMMAND echo LINTING FOR LUA 5.3
	COMMAND luacheck --std=lua53 ${lua_SOURCES}
	COMMAND echo LINTING FOR LUA 5.4
	COMMAND luacheck --std=lua54 ${lua_SOURCES}
)
