PROG		:= mpt-status
PREFIX		:= /usr
CFLAGS		:= -Wall -W -O2
DFLAGS		:=
LDFLAGS		:=
DESTDIR		:=
MANDIR		:= /usr/share/man
CC		:= gcc
INSTALL		:= install
ARCH		:= $(shell uname -m)

${PROG}: ${PROG}.c ${PROG}.h Makefile
	${CC} ${DFLAGS} ${CFLAGS} -o $@ $< ${LDFLAGS}

install: ${PROG}
	${INSTALL} -d ${DESTDIR}${PREFIX}/sbin/
	${INSTALL} -o root -g root -m 0500 $< ${DESTDIR}${PREFIX}/sbin

install_doc: man/${PROG}.8
	${INSTALL} -d ${DESTDIR}${MANDIR}/man8/
	${INSTALL} -o root -g root -m 0644 $< ${DESTDIR}${MANDIR}/man8
	gzip -9 ${DESTDIR}${MANDIR}/man8/${PROG}.8

uninstall:
	\rm -f ${DESTDIR}${PREFIX}/sbin/${PROG}

uninstall_doc:
	\rm -f ${DESTDIR}${MANDIR}/man8/${PROG}.8.gz

clean:
	\rm -f ${PROG}

distclean: clean
	\rm -f core* *~

