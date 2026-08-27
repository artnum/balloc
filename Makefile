DEBUG ?= 1
CC=gcc
CLANG=clang-21

ifeq ($(DEBUG),1)
CFLAGS=-ggdb -Wall -Wextra -pedantic -O0
LIBS=-ggdb
else
CFLAGS=-O2 -DNDEBUG -march=native
LIBS=
endif
SRCFILES=src/balloc.c
OBJFILES=$(addprefix build/, $(addsuffix .o,$(basename $(notdir $(SRCFILES)))))
RM=rm -Rf
NAME=balloc
AR=ar

all: build build/$(NAME).a

build:
	mkdir -p $@

build/$(NAME).a: build/balloc.o
	$(AR) rcs $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: test.c $(SRCFILES)
	$(CLANG) -fsanitize-address-use-after-return=always -fsanitize=address -fno-omit-frame-pointer  $(CFLAGS) test.c $(SRCFILES) -o test
	ASAN_OPTIONS=detect_leaks=1 ASAN_OPTIONS=detect_stack_use_after_return=1 ./test

clean:
	$(RM) $(wildcard $(OBJFILES) $(NAME)) build/$(NAME).a vgcore.* test
