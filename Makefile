PROG=	orch
WARNS?=	6

SRCS=	orch.c	\
		orch_interp.c \
		orch_lua.c

CFLAGS+=	-I/usr/local/include/lua54
LDFLAGS+=	-L/usr/local/lib -llua-5.4

.if make(lint) && !exists(/usr/local/bin/luacheck)
.error "linting requires devel/lua-luacheck"
.endif
lint:
	luacheck orch.lua

.include <bsd.prog.mk>
