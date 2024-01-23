.include <bsd.own.mk>

MAN=	orch.5

MINSTALL?=	${INSTALL} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}

COMPRESS_CMD?=	gzip -cf
COMPRESS_EXT?=	.gz

.MAIN: all

.for file in ${MAN}
MANFILES+=	${file}${COMPRESS_EXT}
CLEANFILES+=	${file}${COMPRESS_EXT}
all: ${file}${COMPRESS_EXT}
${file}${COMPRESS_EXT}: ${file}
	cat ${file} | ${COMPRESS_CMD} > ${.TARGET}
.endfor

clean:
	rm -f ${CLEANFILES}

install: ${MANFILES}
.for file in ${MAN}
	mkdir -p ${DESTDIR}${MANDIR}${file:E}
	${MINSTALL} ${.CURDIR}/${file}${COMPRESS_EXT} ${DESTDIR}${MANDIR}${file:E}
.endfor
