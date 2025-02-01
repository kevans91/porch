#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

option(EXAMPLES "Install examples" ON)
option(MANPAGES "Install manpages" ON)

if(EXAMPLES)
	add_subdirectory(examples)
endif()
add_subdirectory(lua)
if(MANPAGES)
	add_subdirectory(man)
endif()
