VERSION = 0.1

PREFIX = /usr/local
LDFLAGS = -lm -lpng
CFLAGS = -Wall -O3
OBJ = colors.o
BIN = colors

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJ) $(LDFLAGS)

colors.o: queue.h

clean:
	rm -f $(BIN) $(OBJ)
