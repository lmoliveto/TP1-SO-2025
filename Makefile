SHELL := /bin/bash

TARGETS=$(wildcard ./src/*.c)
UTILS=$(wildcard ./src/utils/*.c)
EXES=$(TARGETS:.c=)
UTILS_OBJ=$(UTILS:.c=.o)
CC=gcc
GCC_ASAN_PRELOAD=$(shell gcc -print-file-name=libasan.so)
CFLAGS=-Wall -g -std=c99 -fsanitize=address -D_XOPEN_SOURCE=500 -I"src/headers"
VALGRIND_CFLAGS=$(filter-out -fsanitize=address, $(CFLAGS))
LFLAGS=-lm

STRATEGIES := alpha up neighb
STRATEGY_TARGETS := $(addprefix player_,$(STRATEGIES))

# Compile each .c into a its corresponding executable -- without compiling in other .c's
all: $(EXES) $(STRATEGY_TARGETS)

$(EXES) : % : %.c $(UTILS_OBJ)
	$(CC) $(CFLAGS) $< $(UTILS_OBJ) $(LFLAGS) -o $(notdir $@) 

$(UTILS_OBJ) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

player_%: ./src/player.c $(UTILS_OBJ)
	$(CC) $(CFLAGS) $< $(UTILS_OBJ) -o $@ -DSTRATEGY_`echo $* | tr '[:lower:]' '[:upper:]'` $(LFLAGS)

clean:
	@find . -name "*.dSYM" -type d -exec rm -r {} + 2>/dev/null || true
	@find . -name "*.o" -type f -exec rm {} + 2>/dev/null || true
	@rm $(notdir $(EXES)) $(STRATEGY_TARGETS) 2>/dev/null || true

warnings:
	@make clean --no-print-directory
	@make CFLAGS="$(VALGRIND_CFLAGS)" all --no-print-directory
	
	@valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
		 --trace-children=yes \
		 -s \
         ./Master -v ./view -p ./player ./player

style:
	@clang-format -i ./**/*.c

.PHONY: clean style warnings