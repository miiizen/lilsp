build: 
	cc -std=c99 -Wall lilsp.c mpc.c -ledit -lm  -o bin/lilsp
debug:
	cc -std=c99 -Wall -g lilsp.c mpc.c -ledit -lm -o bin/lilsp
clean:
	rm -f bin/*