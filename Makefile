PROG=	orch
MAN=

SRCS=	orch.c	\
		orch_interp.c \
		orch_lua.c

CFLAGS+=	-I/usr/local/include/lua54
LDFLAGS+=	-L/usr/local/lib -llua-5.4

.include <bsd.prog.mk>
