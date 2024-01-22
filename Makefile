
SUBDIR+=	src

# For compatibility with earlier versions, make install in the root just does
# the src install, which installs orch(1) and orch.lua into
# /usr/local/share/orch.  Other consumers like just want to build and install
# out of lib/.  Odds are, you really only want one or the other and not this
# top-level Makefile.
.if !make(install)
SUBDIR+=	lib
.endif

.include <bsd.subdir.mk>
