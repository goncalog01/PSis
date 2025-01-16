all:
	protoc-c --c_out=. lizardsNroachesNwasps.proto
	$(MAKE) -C lizardsNroachesNwasps-server
	$(MAKE) -C lizard-client
	$(MAKE) -C roaches-client
	$(MAKE) -C wasps-client

clean:
	$(MAKE) -C lizardsNroachesNwasps-server clean
	$(MAKE) -C lizard-client clean
	$(MAKE) -C roaches-client clean
	$(MAKE) -C wasps-client clean