C=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = server.o client.o

all: server client

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: server.o
	$(CC) -o $@ $^ $(CFLAGS)
client: client.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	-rm server client $(OBJ)
