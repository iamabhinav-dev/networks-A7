CC = gcc
CFLAGS = -Wall -Wextra

all: cldp_server cldp_client

cldp_server: cldp_server.c cldp_common.h
	$(CC) $(CFLAGS) -o cldp_server cldp_server.c

cldp_client: cldp_client.c cldp_common.h
	$(CC) $(CFLAGS) -o cldp_client cldp_client.c

clean:
	rm -f cldp_server cldp_client