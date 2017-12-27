GCC = gcc -Werror -Wall -Wshadow

ref:
	git fetch origin
	git reset --hard origin/master

all:
	$(GCC) httpserver.cramerg.c -o httpserver

httpclient.cramerg.o:
	$(GCC) -c httpserver.cramerg.c

db:
	$(GCC) -g httpserver.cramerg.c -o debug.o
	lldb debug.o -- 5000

test: all
	./httpserver 5000

memory: all
	valgrind -v --tool=memcheck --show-reachable=yes --leak-check=full ./httpserver 5000

clean:
	rm -f *.o httpserver httpserver.prof *.out core.*
	rm -r -f *.dSYM
