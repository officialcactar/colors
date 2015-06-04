VERSION = 0.1

PREFIX = /usr/local
LDFLAGS = -L/usr/local/lib -lm -lpng
CPPFLAGS = -I/usr/local/include
CFLAGS = -Wall -O3
OBJ = colors.o png.o
BIN = colors

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(OBJ) $(LDFLAGS)

colors.o: arg.h colors.h queue.h
png.o: arg.h colors.h queue.h

clean:
	rm -f $(BIN) $(OBJ)
