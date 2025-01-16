all: wasps-client.c ../lizardsNroachesNwasps.pb-c.h
	gcc-11 ../lizardsNroachesNwasps.pb-c.c -g -o wasps-client wasps-client.c -lncurses -lzmq -lprotobuf-c

clean:
	rm wasps-client