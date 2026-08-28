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

all: build build/$(NAME).a build/$(NAME)-stats.a

build:
	mkdir -p $@

build/$(NAME).a: build/balloc.o 
	$(AR) rcs $@ $^

build/$(NAME)-stats.a: build/balloc-stats.o 
	$(AR) rcs $@ $^

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build/$(NAME)-stats.o: src/balloc.c
	$(CC) $(CFLAGS) -D BALLOC_STATS -c $< -o $@

test: test.c $(SRCFILES)
	echo "Classic run"
	$(CLANG) -fsanitize-address-use-after-return=always -fsanitize=address -fno-omit-frame-pointer  $(CFLAGS) test.c $(SRCFILES) -o test1
	ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 ./test1
	echo "Force mmap usage"
	$(CLANG) -DBALLOC_MMAP_TRIGGER_SIZE=0 -fsanitize-address-use-after-return=always -fsanitize=address -fno-omit-frame-pointer  $(CFLAGS) test.c $(SRCFILES) -o test2
	ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 ./test2
	echo "Disable mmap support"
	$(CLANG) -DBALLOC_HAVE_MMAP=0 -fsanitize-address-use-after-return=always -fsanitize=address -fno-omit-frame-pointer  $(CFLAGS) test.c $(SRCFILES) -o test3
	ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 ./test3

clean: 
	$(RM) $(wildcard $(OBJFILES) $(NAME)) build/$(NAME).a vgcore.* test1 test2 test3
