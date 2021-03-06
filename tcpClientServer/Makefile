# This is a sample Makefile which compiles source files named:
# - tcpechotimeserv.c
# - tcpechotimecliv.c
# - time_cli.c
# - echo_cli.c
# and creating executables: "server", "client", "time_cli"
# and "echo_cli", respectively.
#
# It uses various standard libraries, and the copy of Stevens'
# library "libunp.a" in ~cse533/Stevens/unpv13e_solaris2.10 .
#
# It also picks up the thread-safe version of "readline.c"
# from Stevens' directory "threads" and uses it when building
# the executable "server".
#
# It is set up, for illustrative purposes, to enable you to use
# the Stevens code in the ~cse533/Stevens/unpv13e_solaris2.10/lib
# subdirectory (where, for example, the file "unp.h" is located)
# without your needing to maintain your own, local copies of that
# code, and without your needing to include such code in the
# submissions of your assignments.
#
# Modify it as needed, and include it with your submission.

CC = gcc

LIBS = -lresolv -lnsl -lsocket -lpthread \
	lib/lib.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -Ilib/

all: archive_lib client server echo_cli time_cli

compile_lib: 
	cd lib; ${CC} ${CFLAGS} -c *.c; cd ..

archive_lib: compile_lib
	ar -cvq lib/lib.a lib/readn.o lib/writen.o lib/readline.o

time_cli: time_cli.o
	${CC} ${FLAGS} -o time_cli time_cli.o ${LIBS}
time_cli.o: time_cli.c
	${CC} ${CFLAGS} -c time_cli.c

echo_cli: echo_cli.o
	${CC} ${FLAGS} -o echo_cli echo_cli.o ${LIBS}
echo_cli.o: echo_cli.c
	${CC} ${CFLAGS} -c echo_cli.c


# server uses the thread-safe version of readline.c

server: tcpechotimesrv.o readline.o
	${CC} ${FLAGS} -o server tcpechotimesrv.o readline.o ${LIBS}
tcpechotimesrv.o: tcpechotimesrv.c
	${CC} ${CFLAGS} -c tcpechotimesrv.c

client: tcpechotimecli.o
	${CC} ${FLAGS} -o client tcpechotimecli.o ${LIBS}
tcpechotimecli.o: tcpechotimecli.c
	${CC} ${CFLAGS} -c tcpechotimecli.c


# pick up the thread-safe version of readline.c from directory "threads"

readline.o: lib/readline.c
	${CC} ${CFLAGS} -c lib/readline.c


clean: 
	rm echo_cli echo_cli.o server tcpechotimesrv.o client tcpechotimecli.o time_cli time_cli.o lib/*.o lib/*.a


