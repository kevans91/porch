
PROG=	orch
WARNS?=	6

.PATH:	${.CURDIR}/../man

SRCS=	orch.c		\
	orch_compat.c	\
	orch_interp.c

.if empty(ORCHLUA_PATH)
# This is maybe a little convoluted, but if ORCHLUA_PATH isn't populated then
# we'll just let the lib/ build install it into LUA_MODSHAREDIR and we'll use
# that.  If it is populated, then we'll install orch.lua into it ourselves.
ORCHLUA_PATH?=	${LUA_MODSHAREDIR}
.else
FILESGROUPS+=	FILES
FILES=		orch.lua
FILESDIR=	${ORCHLUA_PATH}
.endif

CFLAGS+=	-DORCHLUA_PATH=\"${ORCHLUA_PATH}\"

# Build the module into the orch(1) driver
.PATH:	${.CURDIR}/../lib
.include "${.CURDIR}/../lib/Makefile.inc"

.include "${.CURDIR}/../examples/Makefile.inc"

.include <bsd.prog.mk>
