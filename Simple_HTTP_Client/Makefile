GCC = gcc -Werror -Wall -Wshadow

ref:
	git fetch origin
	git reset --hard origin/master

all:
	$(GCC) httpclient.cramerg.c -o httpclient

httpclient.cramerg.o:
	$(GCC) -c httpclient.cramerg.c

db:
	$(GCC) -g httpclient.cramerg.c -o debug.o
	lldb debug.o -- ee347lnx4.ecn.purdue.edu 80 /ece463/lab1/test_short.txt

test: all
	./httpclient ee347lnx4.ecn.purdue.edu 80 /ece463/lab1/test_short.txt > output1.txt
	./httpclient ee347lnx4.ecn.purdue.edu 80 /ece463/lab1/test_very_long.txt > output2.txt

memory: all
	valgrind -v --tool=memcheck --show-reachable=yes --leak-check=full ./httpclient ee347lnx4.ecn.purdue.edu 80 /ece463/lab1/test_very_long.txt

clean:
	rm -f *.o httpclient httpclient.prof *.out core.* output*.txt
	rm -r -f *.dSYM
