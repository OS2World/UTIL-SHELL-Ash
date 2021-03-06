#	@(#)Makefile	8.4 (Berkeley) 5/5/95
#	$Id: Makefile,v 1.15.2.3 1997/10/26 22:46:55 jkh Exp $

PROG=	sh
SHSRCS=	alias.c cd.c echo.c error.c eval.c exec.c expand.c \
	histedit.c input.c jobs.c mail.c main.c memalloc.c miscbltin.c \
	mystring.c options.c output.c parser.c printf.c redir.c show.c \
	trap.c var.c
GENSRCS= arith.c arith_lex.c builtins.c init.c nodes.c syntax.c
GENHDRS= builtins.h nodes.h syntax.h token.h
SRCS= ${SHSRCS} ${GENSRCS} ${GENHDRS}

DPADD+= ${LIBL} ${LIBEDIT} ${LIBTERMCAP}
LDADD+= -ll -ledit -ltermcap

LFLAGS= -8	# 8-bit lex scanner for arithmetic
CFLAGS+=-DSHELL -I. -I${.CURDIR}
# for debug:
# CFLAGS+= -g -DDEBUG=2

.PATH:	${.CURDIR}/bltin ${.CURDIR}/../../usr.bin/printf

CLEANFILES+= mkinit mkinit.o mknodes mknodes.o \
	mksyntax mksyntax.o \
	y.tab.h
CLEANFILES+= ${GENSRCS} ${GENHDRS}


.ORDER: builtins.c builtins.h
builtins.c builtins.h: mkbuiltins builtins.def
	cd ${.CURDIR}; sh mkbuiltins ${.OBJDIR}

init.c: mkinit alias.c eval.c exec.c input.c jobs.c options.c parser.c \
	redir.c trap.c var.c
	./mkinit ${.ALLSRC:S/^mkinit$//}

# XXX this is just to stop the default .c rule being used, so that the
# intermediate object has a fixed name.
# XXX we have a default .c rule, but no default .o rule.
.o:
	${CC} ${CFLAGS} ${LDFLAGS} ${.IMPSRC} ${LDLIBS} -o ${.TARGET}
mkinit: mkinit.o
mkinit.o: mkinit.c		# XXX and many headers
mknodes: mknodes.o
mknodes.o: mknodes.c		# XXX and many headers
mksyntax: mksyntax.o
mksyntax.o: mksyntax.c		# XXX and many headers

.ORDER: nodes.c nodes.h
nodes.c nodes.h: mknodes nodetypes nodes.c.pat
	./mknodes ${.CURDIR}/nodetypes ${.CURDIR}/nodes.c.pat

.ORDER: syntax.c syntax.h
syntax.c syntax.h: mksyntax
	./mksyntax

token.h: mktokens
	sh ${.CURDIR}/mktokens

y.tab.h: arith.c

# Rules for object files that rely on generated headers.
arith_lex.o: y.tab.h
cd.o: nodes.h
eval.o: builtins.h nodes.h syntax.h
exec.o: builtins.h nodes.h syntax.h
expand.o: nodes.h syntax.h
input.o: syntax.h
jobs.o: nodes.h syntax.h
main.o: nodes.h
mystring.o: syntax.h
options.o: nodes.h
output.o: syntax.h
parser.o: nodes.h syntax.h token.h
redir.o: nodes.h
show.o: nodes.h
trap.o: nodes.h syntax.h
var.o: nodes.h syntax.h

.include <bsd.prog.mk>
