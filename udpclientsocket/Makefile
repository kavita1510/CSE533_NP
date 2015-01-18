# The Makefile which will make the server and client executables.
# It is linked to the libunp on Tapti's local machine. Please change to point
# to required machine. 
# Also, it links to the threadsafe version of readline from Steven's code.

LIBUNP_DIR = /home/courses/cse533/Stevens/unpv13e_solaris2.10
LIBS = -lresolv -lnsl -lsocket -lpthread -lm\
	${LIBUNP_DIR}/libunp.a

CC = gcc


FLAGS = -g -Wall

CFLAGS = ${FLAGS} -I${LIBUNP_DIR}/lib  # Remove the -DFUP_LOG to disable logging.

#all: client server testclient
all: client server

# server uses the thread-safe version of readline.c

server: udpserver.o readline.o fup.o server_sliding_window.o get_ifi_info.o prifinfo.o rttcal.o
	${CC} ${FLAGS} -o server udpserver.o readline.o fup.o server_sliding_window.o get_ifi_info.o prifinfo.o rttcal.o ${LIBS}
udpserver.o: udpserver.c udpserver.h
	${CC} ${CFLAGS} -c udpserver.c

client: udpclient.o fup.o client_sliding_window.o get_ifi_info.o prifinfo.o
	${CC} ${FLAGS} -o client udpclient.o fup.o client_sliding_window.o get_ifi_info.o prifinfo.o ${LIBS}
udpclient.o: udpclient.c udpclient.h
	${CC} ${CFLAGS} -c udpclient.c

fup.o: fup.c fup.h
	${CC} ${CFLAGS} -c fup.c

client_sliding_window.o: client_sliding_window.c client_sliding_window.h 
	${CC} ${CFLAGS} -c client_sliding_window.c

server_sliding_window.o: server_sliding_window.c server_sliding_window.h
	${CC} ${CFLAGS} -c server_sliding_window.c

get_ifi_info.o: get_ifi_info.c
	${CC} ${CFLAGS} -c get_ifi_info.c

prifinfo.o: prifinfo.c 
	${CC} ${CFLAGS} -c prifinfo.c

rttcal.o : rttcal.c
	${CC} ${CFLAGS} -c rttcal.c

# pick up the thread-safe version of readline.c from directory "threads"

readline.o: ${LIBUNP_DIR}/threads/readline.c
	${CC} ${CFLAGS} -c ${LIBUNP_DIR}/threads/readline.c

# client_win: client_sliding_window.o
# 	${CC} ${CFLAGS} -o client_win client_sliding_window.o

# server_win: server_sliding_window.o
# 	${CC} ${CFLAGS} -o server_win server_sliding_window.o

clean:
	rm server client udpclient.o udpserver.o fup.o readline.o prifinfo.o get_ifi_info.o client_sliding_window.o server_sliding_window.o rttcal.o

