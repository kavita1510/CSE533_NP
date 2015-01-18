# The Makefile for the ODR, server and client
CC = gcc
HOSTNAME := $(shell hostname)
ifeq ($(HOSTNAME),minix)
	LIBUNP_DIR = /users/cse533/Stevens/unpv13e
	FLAGS = -g -O0 -Wall -I${LIBUNP_DIR}/lib -D WK_OFN=\"/home/tpalit/odr.socket\" -D WK_SFN=\"/home/tpalit/server.socket\" # The wellknown file names
else ifeq ($(HOSTNAME),scooter)
	LIBUNP_DIR =  /home/kavita/course_work/cse533/unpv13e_linux
	FLAGS = -g -O0 -Wall -I${LIBUNP_DIR}/lib -D WK_OFN=\"/home/kavita/course_work/cse533/odr.socket\" -D WK_SFN=\"/home/kavita/course_work/cse533/server.socket\"# The wellknown file names
else 
	LIBUNP_DIR = /home/tapti/Documents/NP/unpv13e
	FLAGS = -g -O0 -Wall -I${LIBUNP_DIR}/lib -D WK_OFN=\"/home/tapti/odr.socket\" -D WK_SFN=\"/home/tapti/server.socket\"# The wellknown file names
endif

LIBS = -lpthread ${LIBUNP_DIR}/libunp.a




all: ODR_tpalit server_tpalit client_tpalit

ODR_tpalit: get_hw_addrs.o odr.o
	${CC} ${FLAGS} -o ODR_tpalit odr.o get_hw_addrs.o  ${LIBS}

odr.o:	odr.c odr.h api.h
	${CC} ${FLAGS} -c odr.c 

get_hw_addrs.o: get_hw_addrs.c hw_addrs.h
	${CC} ${FLAGS} -c get_hw_addrs.c

client_tpalit:	client.o api.o
	${CC} ${FLAGS} -o client_tpalit client.o api.o

client.o: client.c api.h 
	${CC} ${FLAGS} -c client.c

server_tpalit:	time_server.o api.o
	${CC} ${FLAGS} -o server_tpalit time_server.o api.o

api.o: api.c api.h
	${CC} ${FLAGS} -c api.c

time_server.o: time_server.c api.h
	${CC} ${FLAGS} -c time_server.c

clean:
	rm get_hw_addrs.o odr.o ODR_tpalit client.o client_tpalit time_server.o server_tpalit api.o
