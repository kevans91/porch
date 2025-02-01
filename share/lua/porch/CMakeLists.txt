#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

file(GLOB share_porch_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.lua)
install(FILES ${share_porch_SOURCES}
	DESTINATION "${LUA_MODSHAREDIR}/porch")
