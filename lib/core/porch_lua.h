/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "porch.h"
#include "porch_lib.h"

#define	ORCHLUA_PROCESSHANDLE	"porchlua_process"

void porchlua_register_process_metatable(lua_State *L);
