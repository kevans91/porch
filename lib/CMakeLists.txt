add_subdirectory(core)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua"
	DESTINATION "${CMAKE_INSTALL_PREFIX}${LUA_MODSHAREDIR}")
add_custom_target(lint
	COMMAND luacheck "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua")
