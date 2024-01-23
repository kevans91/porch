
SUBDIR+=	lib
SUBDIR+=	man
SUBDIR+=	src

lint:
	$(MAKE) -C lib lint

# Just pass this on for now.
check:
	env ORCHLUA_PATH=${.CURDIR}/lib $(MAKE) -C tests check

.include <bsd.subdir.mk>
