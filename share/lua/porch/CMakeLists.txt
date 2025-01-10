file(GLOB share_porch_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.lua)
install(FILES ${share_porch_SOURCES}
	DESTINATION "${LUA_MODSHAREDIR}/porch")
