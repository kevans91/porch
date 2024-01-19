PROG=	orch
WARNS?=	6

SRCS=	orch.c	\
	orch_interp.c \
	orch_lua.c

.if ${.MAKE.OS} == "Linux"
CFLAGS+=	-D_GNU_SOURCE
.endif
.if ${.MAKE.OS} == "Linux" || ${.MAKE.OS} == "Darwin"
SRCS+=	orch_compat.c
.endif

.if !empty(ORCHLUA_PATH)
CFLAGS+=	-DORCHLUA_PATH=\"${ORCHLUA_PATH}\"

.if empty(ORCHLUA_PATH:M/*)
.error "ORCHLUA_PATH must be empty or absolute"
.endif

FILESGROUPS+=	FILES
FILES=		orch.lua
FILESDIR=	${ORCHLUA_PATH}
.endif

LUA_INCDIR?=	/usr/local/include/lua54
LUA_LIB?=	-L/usr/local/lib -llua-5.4

CFLAGS+=	-I${LUA_INCDIR}
LDADD+=	${LUA_LIB}

.if make(lint) && !exists(/usr/local/bin/luacheck)
.error "linting requires devel/lua-luacheck"
.endif
lint:
	luacheck orch.lua

.include "examples/Makefile.inc"

.include <bsd.prog.mk>
