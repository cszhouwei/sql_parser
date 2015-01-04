all: test

test: test.cc sql_parser.h
	g++ -g -Wall -O2 -o test test.cc

clean:
	@rm *.o
