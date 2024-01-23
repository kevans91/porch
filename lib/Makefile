
SHLIB_NAME=	core.so
SHLIBDIR?=	${LUA_MODLIBDIR}/orch

# The lib build will install orch.lua into share/lua
FILES=	orch.lua
FILESDIR=	${LUA_MODSHAREDIR}

.if make(lint) && !exists(/usr/local/bin/luacheck)
.error "linting requires devel/lua-luacheck"
.endif
lint:
	luacheck orch.lua

.include "Makefile.inc"
.include <bsd.lib.mk>
