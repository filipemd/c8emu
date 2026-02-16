#
#	Copyright (C) 2025 filipemd
#
#	This file is part of C8EMU.
#
#	C8EMU is free software: you can redistribute it and/or modify it under the terms of the 
#	GNU General Public License as published by the Free Software Foundation, either version 3 
#	of the License, or (at your option) any later version.
#
#	C8EMU is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without 
#	even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
#	GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License along with C8EMU. If not,
#	see <https://www.gnu.org/licenses/>. 
#

# --- Configuração do Projeto ---
TARGET   := bin/c8emu
SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin

# --- Configurações do Compilador e Linker ---
CC       := gcc

# O SDL3 geralmente requer o pkg-config para encontrar os cabeçalhos (.h) e bibliotecas (.so/.lib)
FLAGS  := $(shell pkg-config --cflags sdl3)
LIBS   := $(shell pkg-config --libs sdl3) -lm

# CFLAGS: 
CFLAGS   := -Wall -Wextra -Wpedantic -std=c99 $(FLAGS) -I$(SRC_DIR) -MMD -MP
LDFLAGS  := $(LIBS)

# DEBUG
ifeq ($(DEBUG), 1)
	CFLAGS+=-O0 -g -DDEBUG
else
	CFLAGS+=-O2
endif

SRCS     := src/main.c src/emulator.c src/beep.c
# Mapeia src/arquivo.c para obj/arquivo.o
OBJS     := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
# Mapeia obj/arquivo.o para obj/arquivo.d (arquivos de dependência)
DEPS     := $(OBJS:.o=.d)

# --- Regras de Compilação ---

.PHONY: all clean run

# Alvo principal
all: $(TARGET)

test:
	@$(MAKE) clean > /dev/null
	@$(MAKE) CFLAGS="$(CFLAGS) -DTEST" SRCS="src/emulator.c src/unity.c src/test.c" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined" > /dev/null
	@./$(TARGET)
	@$(MAKE) clean > /dev/null

# Regra para vincular (link) o executável final
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Regra para compilar os arquivos fonte (.c) em objetos (.o)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Cria os diretórios necessários caso não existam
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Inclui os arquivos de dependência gerados pelo compilador
# Isso garante que, se você mudar um .h, os .c que o usam serão recompilados
-include $(DEPS)

# --- Regras Utilitárias ---

# Limpa os arquivos temporários e o executável
clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)

# Compila e executa o programa imediatamente
run: all
	@./$(TARGET)