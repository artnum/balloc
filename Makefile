DEBUG ?= 1
CC=gcc

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

all: build/$(NAME).a

#$(NAME): build/$(NAME).a
#	$(CC) $^ -o $(NAME) $(LIBS)

build/$(NAME).a: build/balloc.o
	$(AR) rcs $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(wildcard $(OBJFILES) $(NAME)) build/$(NAME).a vgcore.*
