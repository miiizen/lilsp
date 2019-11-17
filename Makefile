INCLUDE=include
BIN=bin
build: 
	cc -std=c99 -Wall lilsp.c $(INCLUDE)/mpc.c -ledit -lm  -o $(BIN)/lilsp
debug:
	cc -std=c99 -Wall -g -O0 lilsp.c $(INCLUDE)/mpc.c -ledit -lm -o $(BIN)/lilsp
clean:
	rm -f bin/*