TARGETS=$(wildcard ./src/*.c)
EXES=$(TARGETS:.c=)
CC=gcc
CFLAGS=-Wall -g -std=c99 -fsanitize=address -D_XOPEN_SOURCE=500

# Compile each .c into a its corresponding executable -- without compiling in other .c's
all: $(EXES)

$(EXES) : % : %.c
	$(CC) $(CFLAGS) $< -o $(notdir $@)

clean: clean_intermediates
	@rm $(notdir $(EXES)) &> /dev/null

clean_intermediates:
	@rm -r ./**/*.dSYM > /dev/null 2>/dev/null || true
	@rm ./**/*.o > /dev/null 2>/dev/null || true

warnings:
	valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         ./Master

.PHONY: clean clean_intermediates warnings