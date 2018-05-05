CC=gcc

CFLAGS = -O3
LFLAGS = -lpthread

all: udp-server udp-client udp-nat

udp-server: udp-server.o
	$(CC) $(LFLAGS) -o $@ $?

udp-client: udp-client.o
	$(CC) $(LFLAGS) -o $@ $?

udp-nat: udp-nat.o
	$(CC) $(LFLAGS) -o $@ $?

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $?

clean:
	@rm -f *.o udp-server udp-client udp-nat

