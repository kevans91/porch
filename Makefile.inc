# Common to all parts

CFLAGS+=	-I${.CURDIR}/../include
.if defined(.MAKE.OS) && ${.MAKE.OS} == "Linux"
CFLAGS+=	-D_GNU_SOURCE
.endif
.if defined(.MAKE.OS) && ${.MAKE.OS} == "NetBSD"
CWARNFLAGS+=	-Wno-conversion -Wno-maybe-uninitialized
.endif

LUA_INCDIR?=	/usr/local/include/lua54
LUA_MODLIBDIR?=	/usr/local/lib/lua/5.4
LUA_MODSHAREDIR?=	/usr/local/share/lua/5.4
LUA_LIB?=	-L/usr/local/lib -llua-5.4

LDADD+=	${LUA_LIB} -lm
CFLAGS+=	-I${LUA_INCDIR}
