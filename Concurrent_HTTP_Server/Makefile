GCC = gcc -Werror -Wall -Wshadow
PORT = 5001

ref:
	git fetch origin
	git reset --hard origin/master

all:
	$(GCC) httpserver_fork.cramerg.c -o httpserver_fork

httpclient.cramerg.o:
	$(GCC) -c httpserver_fork.cramerg.c

db:
	$(GCC) -g httpserver_fork.cramerg.c -o debug.o
	lldb debug.o -- $(PORT)

test: all
	./httpserver_fork $(PORT)

memory: all
	valgrind -v --tool=memcheck --show-reachable=yes --leak-check=full ./httpserver_fork $(PORT)

clean:
	rm -f *.o httpserver_fork httpserver_fork.prof *.out core.*
	rm -r -f *.dSYM
