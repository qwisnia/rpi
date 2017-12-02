all: main
main: dht.c
	gcc -std=c99 -Wall dht.c -o db -lsqlite3 -o dht -lwiringPi -D_GNU_SOURCE -lrt -O3 -H
