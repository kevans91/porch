
file(GLOB orch_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

set(warnings "-Wall -Wextra -Werror")

add_executable(orch ${orch_SOURCES})

set(orch_INCDIRS "${CMAKE_SOURCE_DIR}/include" "${LUA_INCLUDE_DIR}")
target_include_directories(orch PRIVATE ${orch_INCDIRS})
target_link_libraries(orch core_static "${LUA_LIBRARIES}")

#install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/orch.lua"
#	DESTINATION "${CMAKE_INSTALL_PREFIX}${LUA_MODSHAREDIR}")
install(TARGETS orch
	DESTINATION "${CMAKE_INSTALL_PREFIX}${ORCHLUA_BINDIR}")
