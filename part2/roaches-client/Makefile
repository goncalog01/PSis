all: roaches-client.c ../lizardsNroachesNwasps.pb-c.h
	gcc-11 ../lizardsNroachesNwasps.pb-c.c -g -o roaches-client roaches-client.c -lncurses -lzmq -lprotobuf-c

clean:
	rm roaches-client