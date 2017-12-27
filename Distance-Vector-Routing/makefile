CFLAGS = -g -Wall
CC = gcc
ROUTER_ID = 0
HOSTNAME = Geoffreys-MacBook-Pro-2.local
NE_PORT = 9999
ROUTER_PORT = 9991

# change based on type of router to be built
# value can be either DISTVECTOR or PATHVECTOR
ROUTERMODE = DISTVECTOR

# if DEBUG is 1, debugging messages are printed
DEBUG = 1

# Check which OS
OS := $(shell uname)
ifeq ($(OS), SunOS)
	SOCKETLIB = -lsocket
endif

all : router unit-test

endian.o   :   ne.h endian.c
	$(CC) $(CFLAGS) -D $(ROUTERMODE) -c endian.c

routingtable.o   :   ne.h routingtable.c
	$(CC) $(CFLAGS) -D $(ROUTERMODE) -c routingtable.c

router  :   endian.o routingtable.o router.c
	$(CC) $(CFLAGS) -D $(ROUTERMODE) -D DEBUG=$(DEBUG) endian.o routingtable.o router.c -o router -lnsl $(SOCKETLIB)

unit-test  : routingtable.o unit-test.c
	$(CC) $(CFLAGS) -D $(ROUTERMODE) -D DEBUG=$(DEBUG) routingtable.o unit-test.c -o unit-test -lnsl $(SOCKETLIB)
	./unit-test

router-test: router
	./router 0 $(HOSTNAME) $(NE_PORT) $(ROUTER_PORT)
	./router 1 $(HOSTNAME) $(NE_PORT) $(ROUTER_PORT+1)
	./router 2 $(HOSTNAME) $(NE_PORT) $(ROUTER_PORT+2)
	./router 3 $(HOSTNAME) $(NE_PORT) $(ROUTER_PORT+3)

ne-test:
	./ne $(NE_PORT) 4_routers.conf

clean :
	rm -f *.o
	rm -f router
	rm -f unit-test
