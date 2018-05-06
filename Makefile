CC=gcc

CFLAGS = -O3
LFLAGS = -lpthread

all: udp-server udp-client

udp-server: udp.o udp-server.o
	$(CC) $(LFLAGS) -o $@ $?

udp-client: udp.o udp-client.o
	$(CC) $(LFLAGS) -o $@ $?

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $?

clean:
	@rm -f *.o udp-server udp-client udp-nat

