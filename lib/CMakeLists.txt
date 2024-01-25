add_subdirectory(core)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua"
	DESTINATION "${LUA_MODSHAREDIR}")
add_custom_target(lint
	COMMAND echo LINTING FOR LUA 5.3
	COMMAND luacheck --std=lua53 "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua"
	COMMAND echo LINTING FOR LUA 5.4
	COMMAND luacheck --std=lua54 "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua"
)
