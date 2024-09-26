# check-cli and check-lib are intended to run our tests exclusively against the
# build products.
add_custom_target(check-cli
	COMMAND env PORCHBIN="${CMAKE_BINARY_DIR}/src/porch" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/lib" sh "${CMAKE_CURRENT_SOURCE_DIR}/basic_test.sh"
	DEPENDS porch)
add_custom_target(check-lib
	COMMAND env PORCHLIB_PATH="${CMAKE_BINARY_DIR}/lib" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/lib" LUA_VERSION_MAJOR="${LUA_VERSION_MAJOR}" LUA_VERSION_MINOR="${LUA_VERSION_MINOR}" sh "${CMAKE_CURRENT_SOURCE_DIR}/lua_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# check-install does both of the above, but only against an installed porch(1)
# to confirm that the installation is sane.
add_custom_target(check-install
	COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/basic_test.sh"
	COMMAND env LUA_VERSION_MAJOR="${LUA_VERSION_MAJOR}" LUA_VERSION_MINOR="${LUA_VERSION_MINOR}" sh "${CMAKE_CURRENT_SOURCE_DIR}/lua_test.sh"
	WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(check DEPENDS check-cli check-lib)
