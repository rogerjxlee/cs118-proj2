C=gcc
CFLAGS=-I.
DEPS = # header file 
OBJ = server.o client.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

client: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
