GCC = gcc -Werror -Wall -Wshadow
PORT_HTTP = 5001
PORT_UDP = 5002
FILE = multi_service_server
HOST = $(shell hostname)

ref:
	git fetch origin
	git reset --hard origin/master

all:
	$(GCC) $(FILE).cramerg.c -o $(FILE)

multi_service_server.cramerg.o:
	$(GCC) -c $(FILE).cramerg.c

db:
	$(GCC) -g $(FILE).cramerg.c -o debug.o
	gdb --args ./debug.o $(PORT_HTTP) $(PORT_UDP)

test: all
	./$(FILE) $(PORT_HTTP) $(PORT_UDP)

client:
	./sample_client $(HOST) $(PORT_HTTP) /test_very_long.txt $(PORT_UDP) testingUDP

memory: all
	valgrind -v --tool=memcheck --show-reachable=yes --leak-check=full ./$(FILE) $(PORT_HTTP) $(PORT_UDP)

clean:
	rm -f *.o $(FILE) $(FILE).prof *.out core.*
	rm -r -f *.dSYM
