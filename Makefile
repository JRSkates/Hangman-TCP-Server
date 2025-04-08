CC = gcc

SRC = server.c main.c
EXEC = hangman_server

all: $(EXEC)

$(EXEC): $(SRC)
	$(CC) $(SRC) -o $(EXEC)

clean:
	rm -f $(EXEC)
