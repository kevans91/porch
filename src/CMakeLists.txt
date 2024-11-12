
file(GLOB porch_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_compile_definitions(PORCH_VERSION="${CMAKE_PROJECT_VERSION}")
add_compile_definitions(PORCHLUA_PATH="${PORCHLUA_PATH}")

add_executable(porch ${porch_SOURCES})

set(porch_INCDIRS "${CMAKE_SOURCE_DIR}/include" "${LUA_INCLUDE_DIR}")
target_include_directories(porch PRIVATE ${porch_INCDIRS})
target_link_libraries(porch core_static "${LUA_LIBRARIES}")

install(TARGETS porch
	DESTINATION "${PORCHLUA_BINDIR}")
