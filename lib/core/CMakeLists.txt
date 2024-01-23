
file(GLOB core_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

set(warnings "-Wall -Wextra -Werror")
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_GNU_SOURCE")
endif()

add_library(core SHARED ${core_SOURCES})
set_target_properties(core PROPERTIES
	PREFIX "")
add_library(core_static STATIC ${core_SOURCES})

set(core_INCDIRS
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_SOURCE_DIR}/lib"
	"${LUA_INCLUDE_DIR}")

target_include_directories(core PRIVATE ${core_INCDIRS})
# orch(1) will link against the static lib
target_include_directories(core_static PRIVATE ${core_INCDIRS})

target_link_libraries(core "${LUA_LIBRARIES}")

install(TARGETS core
	DESTINATION "${CMAKE_INSTALL_PREFIX}${LUA_MODLIBDIR}/orch")
