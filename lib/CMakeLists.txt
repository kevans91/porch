add_subdirectory(core)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua"
	DESTINATION "${LUA_MODSHAREDIR}")
add_custom_target(lint
	COMMAND luacheck "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua")
