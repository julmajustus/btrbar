# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: jmakkone <jmakkone@student.hive.fi>        +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2025/04/26 22:27:41 by julmajustus       #+#    #+#              #
#    Updated: 2025/08/02 15:03:01 by julmajustus      ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME        = btrbar
SRC_DIR     = src
INC_DIR     = include
OBJ_DIR     = obj
PROT_DIR    = protocols

# Protocol files
PROT_XML     = $(PROT_DIR)/wlr-layer-shell-unstable-v1.xml
PROT_HDR     = $(INC_DIR)/wlr-layer-shell-unstable-v1-client-protocol.h
PROT_C       = $(OBJ_DIR)/wlr-layer-shell-unstable-v1-client-protocol.c
PROT_OBJ     = $(OBJ_DIR)/wlr-layer-shell-unstable-v1-client-protocol.o

PROT_XDG_XML = $(PROT_DIR)/xdg-shell.xml
PROT_XDG_HDR = $(INC_DIR)/xdg-shell-client-protocol.h
PROT_XDG_C   = $(OBJ_DIR)/xdg-shell-client-protocol.c
PROT_XDG_OBJ = $(OBJ_DIR)/xdg-shell-client-protocol.o

PROT_IPC_XML = $(PROT_DIR)/dwl-ipc-unstable-v2.xml
PROT_IPC_HDR = $(INC_DIR)/dwl-ipc-unstable-v2.h
PROT_IPC_C   = $(OBJ_DIR)/dwl-ipc-unstable-v2.c
PROT_IPC_OBJ = $(OBJ_DIR)/dwl-ipc-unstable-v2.o

# Sources and objects
SRC = $(SRC_DIR)/main.c \
      $(SRC_DIR)/bar.c \
      $(SRC_DIR)/tags.c \
      $(SRC_DIR)/blocks.c \
      $(SRC_DIR)/user_blocks.c \
      $(SRC_DIR)/wl_connection.c \
      $(SRC_DIR)/ipc.c \
      $(SRC_DIR)/render.c \
      $(SRC_DIR)/tools.c \
      $(SRC_DIR)/input.c \
      $(SRC_DIR)/systray.c \
      $(SRC_DIR)/systray_watcher.c


OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC)) \
      $(PROT_OBJ) $(PROT_XDG_OBJ) $(PROT_IPC_OBJ)

# Compiler
CC = gcc

# pkg-config
WAYLAND_CFLAGS = $(shell pkg-config --cflags wayland-client)
WAYLAND_LIBS   = $(shell pkg-config --libs wayland-client)
DBUS_CFLAGS    = $(shell pkg-config --cflags dbus-1)
DBUS_LIBS      = $(shell pkg-config --libs dbus-1)

# Build flags
ifndef BUILD
	BUILD = release
endif

ifeq ($(BUILD), debug)
	CFLAGS = -Og -ggdb3 -Wall -Wextra #-fsanitize=address #-Werror 
else ifeq ($(BUILD), release)
	CFLAGS = -Wall -Wextra -O3 -march=native
else
	CFLAGS = -Wall -Wextra -g -fsanitize=address
endif

CFLAGS  += -std=c11 -DWLR_USE_UNSTABLE \
           -I$(PROT_DIR) -I$(INC_DIR) \
           $(WAYLAND_CFLAGS) $(DBUS_CFLAGS)

LDFLAGS = $(WAYLAND_LIBS) $(DBUS_LIBS) -lm 

# Utilities
RM = rm -rf
INSTALL_DIR = /usr/local/bin

.PHONY: all clean fclean re debug release install uninstall

all: $(NAME)

$(PROT_HDR): $(PROT_XML)
	@echo "Generating layer-shell header"
	wayland-scanner client-header $< $@

$(PROT_C): $(PROT_XML)
	@mkdir -p $(OBJ_DIR)
	@echo "Generating layer-shell source"
	wayland-scanner public-code $< $@

$(PROT_OBJ): $(PROT_C) $(PROT_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROT_XDG_HDR): $(PROT_XDG_XML)
	@echo "Generating xdg-shell header"
	wayland-scanner client-header $< $@

$(PROT_XDG_C): $(PROT_XDG_XML)
	@mkdir -p $(OBJ_DIR)
	@echo "Generating xdg-shell source"
	wayland-scanner public-code $< $@

$(PROT_XDG_OBJ): $(PROT_XDG_C) $(PROT_XDG_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PROT_IPC_HDR): $(PROT_IPC_XML)
	@echo "Generating xdg-shell header"
	wayland-scanner client-header $< $@

$(PROT_IPC_C): $(PROT_IPC_XML)
	@mkdir -p $(OBJ_DIR)
	@echo "Generating xdg-shell source"
	wayland-scanner public-code $< $@

$(PROT_IPC_OBJ): $(PROT_IPC_C) $(PROT_IPC_HDR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(PROT_HDR) $(PROT_XDG_HDR) $(PROT_IPC_HDR) 
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	@echo "Cleaning objects..."
	$(RM) $(OBJ_DIR)

fclean: clean
	@echo "Removing binary..."
	$(RM) $(NAME)

re: fclean all

debug:
	$(MAKE) BUILD=debug all

release:
	$(MAKE) BUILD=release all

install: $(NAME)
	@echo "Installing $(NAME) to $(INSTALL_DIR)..."
	cp $(NAME) $(INSTALL_DIR)/$(NAME)
	@echo "Installation complete."

uninstall:
	@echo "Uninstalling $(NAME) from $(INSTALL_DIR)..."
	rm -f $(INSTALL_DIR)/$(NAME)
	@echo "Uninstallation complete."
