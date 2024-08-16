add_custom_target(check
	COMMAND env PORCHBIN="${CMAKE_BINARY_DIR}/src/porch" PORCHLUA_PATH="${CMAKE_SOURCE_DIR}/lib" sh "${CMAKE_CURRENT_SOURCE_DIR}/basic_test.sh")
