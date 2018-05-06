CC          = g++ 
CFLAGS      = -g
#LIBUTP_PATH = /home/actiontec/workspace/code_reading/third_party/libutp/lib
#INCLUDES    = -I${LIBUTP_PATH}
#LIBS        = -L$(LIBUTP_PATH) -lutp -lpthread -lrt
INCLUDES    = -I ./include
LIBS        = -lpthread
TARGET      = all
 
all: server client
 
server: server.cpp
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES)

client: client.cpp
	$(CC) $(CFLAGS) $^ -o $@  $(INCLUDES) $(LIBS)
 
clean:
	-rm  server client

