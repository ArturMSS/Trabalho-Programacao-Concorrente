# Makefile - Simulacao do Centro de Distribuicao Automatizado (IDP)
#
# Alvos:
#   make            compila o projeto (binario ./idp_sim)
#   make run        compila e executa o cenario 1
#   make run N=2    compila e executa o cenario N
#   make clean      remove objetos e binario

CC      := gcc
CFLAGS  := -Wall -Wextra -std=gnu11 -Iinclude -pthread -g
LDFLAGS := -pthread -lncurses

SRC_DIR  := src
OBJ_DIR  := obj
BIN      := idp_sim

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Objetos sem main.o (usado pelo binario de teste)
OBJS_NO_MAIN := $(filter-out $(OBJ_DIR)/main.o,$(OBJS))

TESTE_SRC := tests/teste_fundacao.c
TESTE_BIN := tests/teste_fundacao

# Cenario padrao para 'make run'
N ?= 1

.PHONY: all run clean teste

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

run: all
	./$(BIN) $(N)

teste: $(TESTE_BIN)
	./$(TESTE_BIN)

$(TESTE_BIN): $(TESTE_SRC) $(OBJS_NO_MAIN) | tests
	$(CC) $(CFLAGS) $(TESTE_SRC) $(OBJS_NO_MAIN) -o $@ $(LDFLAGS)

tests:
	mkdir -p tests

clean:
	rm -rf $(OBJ_DIR) $(BIN) $(TESTE_BIN)
