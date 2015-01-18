# The Makefile for the ODR, server and client
CC = gcc
HOSTNAME := $(shell hostname)

ifeq ($(HOSTNAME),minix)
	LIBUNP_DIR = /users/cse533/Stevens/unpv13e
	FLAGS = -g -O0 -Wall -I${LIBUNP_DIR}/lib -D WK_AFN=\"/home/tpalit/arp.socket\" -D WK_SFN=\"/home/tpalit/server.socket\" # The wellknown file names
else ifeq ($(HOSTNAME),scooter)
	LIBUNP_DIR =  /home/kavita/course_work/cse533/unpv13e_linux
	FLAGS = -g -O0 -Wall -I${LIBUNP_DIR}/lib -D WK_AFN=\"/home/kavita/course_work/cse533/arp.socket\" -D WK_SFN=\"/home/kavita/course_work/cse533/server.socket\"# The wellknown file names
else 
	LIBUNP_DIR = /home/tapti/Documents/NP/unpv13e
	FLAGS = -g -O0 -Wall -I${LIBUNP_DIR}/lib -D WK_AFN=\"/home/tapti/arp.socket\" -D WK_SFN=\"/home/tapti/server.socket\"# The wellknown file names
endif

LIBS = -lpthread ${LIBUNP_DIR}/libunp.a

all: tpalit_arp tpalit_tour

tpalit_tour: get_hw_addrs.o tour.o ping.o arp_api.o
	${CC} ${FLAGS} -o tpalit_tour get_hw_addrs.o tour.o ping.o arp_api.o  ${LIBS}
tour.o: tour.c tour.h
	${CC} ${FLAGS} -c tour.c

tpalit_arp: get_hw_addrs.o arp.o
	${CC} ${FLAGS} -o tpalit_arp arp.o get_hw_addrs.o  ${LIBS}

arp.o: arp.c arp.h
	${CC} ${FLAGS} -c arp.c

get_hw_addrs.o: get_hw_addrs.c hw_addrs.h
	${CC} ${FLAGS} -c get_hw_addrs.c

arp_api.o: arp_api.c arp.h
	${CC} ${FLAGS} -c arp_api.c 

test: test.c arp_api.c arp.h
	${CC} ${FLAGS} test.c arp_api.c  -o test

ping.o: ping.c ping.h
	${CC} ${FLAGS} -c ping.c

ping: ping.c ping.h
	${CC} ${FLAGS} ping.c -o ping
clean:
	rm get_hw_addrs.o arp.o tpalit_arp tpalit_tour tour.o ping.o
