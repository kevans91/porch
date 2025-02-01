#
# Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

set(EXAMPLES
	cat.orch
	cat-multi.orch
	nc.orch
)

install(FILES ${EXAMPLES} DESTINATION
	${PORCHLUA_EXAMPLESDIR})
