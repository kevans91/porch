
PROG=	orch
WARNS?=	6

SRCS=	orch.c		\
	orch_compat.c	\
	orch_interp.c

.if !empty(ORCHLUA_PATH)
CFLAGS+=	-DORCHLUA_PATH=\"${ORCHLUA_PATH}\"

.if empty(ORCHLUA_PATH:M/*)
.error "ORCHLUA_PATH must be an absolute path"
.endif

FILESGROUPS+=	FILES
FILES=		orch.lua
FILESDIR=	${ORCHLUA_PATH}
.endif


# Build the module into the orch(1) driver
.PATH:	${.CURDIR}/../lib
.include "${.CURDIR}/../lib/Makefile.inc"

.include "${.CURDIR}/../examples/Makefile.inc"

.include <bsd.prog.mk>
