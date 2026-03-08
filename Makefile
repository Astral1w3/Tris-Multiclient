CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pthread -g
LDFLAGS = -pthread

# Directory
SRC_DIR = .
OBJ_DIR = obj

# File sorgenti
SERVER_SRCS = server.c game.c player.c board.c \
              protocol.c \
              protocol_io.c protocol_commands.c \
              protocol_lobby.c protocol_match.c protocol_session.c
CLIENT_SRCS = client.c
GUI_CLIENT_SRCS = gui_client.c interface.c

# File oggetto
SERVER_OBJS = $(SERVER_SRCS:%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJS = $(CLIENT_SRCS:%.c=$(OBJ_DIR)/%.o)

# Eseguibili
SERVER_TARGET = server
CLIENT_TARGET = client
GUI_CLIENT_TARGET = gui_client

# GTK per gui_client
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0 2>/dev/null)

.PHONY: all clean directories gui

all: directories $(SERVER_TARGET) $(CLIENT_TARGET)

gui: directories $(SERVER_TARGET) $(CLIENT_TARGET) $(GUI_CLIENT_TARGET)

directories:
	@mkdir -p $(OBJ_DIR)

$(SERVER_TARGET): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS)

$(CLIENT_TARGET): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $@ $(LDFLAGS)

$(GUI_CLIENT_TARGET): $(GUI_CLIENT_SRCS)
	@if pkg-config --exists gtk+-3.0 2>/dev/null; then \
		$(CC) $(CFLAGS) $(GTK_CFLAGS) $(GUI_CLIENT_SRCS) -o $@ $(GTK_LIBS) $(LDFLAGS); \
		echo "Client GUI compilato: ./gui_client"; \
	else \
		echo "Errore: GTK3 non trovato. Installa con: sudo apt install libgtk-3-dev"; \
		exit 1; \
	fi

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(SERVER_TARGET) $(CLIENT_TARGET) $(GUI_CLIENT_TARGET)

install: all
	@echo "Compilazione completata!"
	@echo "Esegui './server [porta]' per avviare il server"
	@echo "Esegui './client [host] [porta]' per avviare un client"
