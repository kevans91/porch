file(GLOB test_files
	${CMAKE_CURRENT_SOURCE_DIR}/my_cat
	${CMAKE_CURRENT_SOURCE_DIR}/*.lua
	${CMAKE_CURRENT_SOURCE_DIR}/*.orch
	${CMAKE_CURRENT_SOURCE_DIR}/*.sh)
add_custom_target(check-setup
	COMMAND cp ${test_files} ${CMAKE_CURRENT_BINARY_DIR})

add_executable(openv openv.c)

# check-cli and check-lib are intended to run our tests exclusively against the
# build products.
add_custom_target(check-cli
	COMMAND env PORCHBIN="${CMAKE_BINARY_DIR}/src/porch" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/lib" sh "${CMAKE_CURRENT_BINARY_DIR}/basic_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS check-setup openv porch)
add_custom_target(check-lib
	COMMAND env PORCHLIB_PATH="${CMAKE_BINARY_DIR}/lib" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/lib" LUA_VERSION_MAJOR="${LUA_VERSION_MAJOR}" LUA_VERSION_MINOR="${LUA_VERSION_MINOR}" sh "${CMAKE_CURRENT_BINARY_DIR}/lua_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS check-setup)

# check-install does both of the above, but only against an installed porch(1)
# to confirm that the installation is sane.
add_custom_target(check-install
	COMMAND sh "${CMAKE_CURRENT_BINARY_DIR}/basic_test.sh"
	COMMAND env LUA_VERSION_MAJOR="${LUA_VERSION_MAJOR}" LUA_VERSION_MINOR="${LUA_VERSION_MINOR}" sh "${CMAKE_CURRENT_BINARY_DIR}/lua_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS check-setup)

add_custom_target(check DEPENDS check-cli check-lib)
