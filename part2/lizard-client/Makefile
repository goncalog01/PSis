all: lizard-client.c ../lizardsNroachesNwasps.pb-c.h
	gcc-11 ../lizardsNroachesNwasps.pb-c.c -g -o lizard-client lizard-client.c -lncurses -lzmq -lprotobuf-c

clean:
	rm lizard-client