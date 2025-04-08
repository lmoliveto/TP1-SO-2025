SHELL := /bin/bash

TARGETS=$(wildcard ./src/*.c)
UTILS=$(wildcard ./src/utils/*.c)
EXES=$(TARGETS:.c=)
UTILS_OBJ=$(UTILS:.c=.o)
CC=gcc
GCC_ASAN_PRELOAD=$(shell gcc -print-file-name=libasan.so)
CFLAGS=-Wall -g -std=c99 -fsanitize=address -D_XOPEN_SOURCE=500 -I"src/headers"
LFLAGS=-lm

STRATEGIES := alpha up neighbor
STRATEGY_TARGETS := $(addprefix player_,$(STRATEGIES))

# Compile each .c into a its corresponding executable -- without compiling in other .c's
all: $(EXES) $(STRATEGY_TARGETS)

$(EXES) : % : %.c $(UTILS_OBJ)
	$(CC) $(CFLAGS) $< $(UTILS_OBJ) $(LFLAGS) -o $(notdir $@) 

$(UTILS_OBJ) : %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

player_%: ./src/player.c $(UTILS_OBJ)
	@upper=$(echo $* | tr '[:lower:]' '[:upper:]'); \
	$(CC) $(CFLAGS) $< $(UTILS_OBJ) -o $@ -DSTRATEGY_$$upper $(LFLAGS)

clean: clean_intermediates
	@rm $(notdir $(EXES)) $(STRATEGY_TARGETS) &> /dev/null || true

clean_intermediates:
	@rm -r ./*.dSYM > /dev/null 2>/dev/null || true
	@rm -r ./**/*.dSYM > /dev/null 2>/dev/null || true
	@rm ./**/*.o > /dev/null 2>/dev/null || true
	@rm ./src/**/*.o > /dev/null 2>/dev/null || true

warnings:
# @FILE="./Master" && \
# GCC_ASAN_PRELOAD=`gcc -print-file-name=libasan.so`
	
	@valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
		 --trace-children=yes \
		 -s \
         ./Master -v ./view -p ./player ./player

.PHONY: clean clean_intermediates warnings