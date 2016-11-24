BIN:=ea_receiver
CC=gcc
SRCPATH=.
CFLAGS=-std=c99 -Wall -Wextra -I $(SRCPATH)
LDFLAGS=-lm
INSTALL_PREFIX=/usr/local

all: CFLAGS += -O3
all: $(BIN)

$(BIN): $(BIN).o
	@$(CC) -o $(BIN) $(BIN).o $(LDFLAGS)

$(BIN).o: $(BIN).c
	@$(CC) -c -o $(BIN).o $(BIN).c $(CFLAGS)

.PHONY: clean
clean:
	@rm -f *.o $(BIN)

.PHONY: install
install: $(BIN)
	@install $(BIN) $(INSTALL_PREFIX)/bin

.PHONY: uninstall
uninstall:
	@rm $(INSTALL_PREFIX)/bin/$(BIN)
