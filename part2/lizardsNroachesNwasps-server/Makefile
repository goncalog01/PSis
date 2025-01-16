all: lizardsNroachesNwasps-server.c ../lizardsNroachesNwasps.pb-c.h
	gcc-11 ../lizardsNroachesNwasps.pb-c.c -g -o lizardsNroachesNwasps-server lizardsNroachesNwasps-server.c -lncurses -lzmq -lprotobuf-c -pthread

clean:
	rm lizardsNroachesNwasps-server