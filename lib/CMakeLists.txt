#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

add_subdirectory(core)

add_custom_target(libs
	DEPENDS core)

