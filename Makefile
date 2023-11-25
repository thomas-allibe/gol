CC = gcc
CFLAGS = -std=c2x -Wall -pedantic -Wextra -Wconversion -Werror -g
INCFLAGS = -I /usr/local/include -I ./include
LINKFLAGS = -lm -l:libraylib.a -L /usr/local/lib
EXEC = gol

SRC = $(wildcard src/*.c)
OBJ = $(addprefix obj/,$(notdir $(SRC:.c=.o)))

print:
	@echo $(OBJ)

all: start build/$(EXEC)
	@echo ""
	@echo "********** COMPILATION DONE **********"
	@echo ""

build/$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LINKFLAGS)

obj/%.o: src/%.c
	$(CC) -o $@ -c $< $(CFLAGS) $(INCFLAGS)

#build/$(EXEC): src/main.c
#	$(CC) -o $@ $< $(CFLAGS) $(LIBFLAGS) $(LINKFLAGS)

start:
	@echo ""
	@echo "********** COMPILATION START *********"
	@echo ""

#
#  CLEANUP
#

#.PHONY clean-build clean-obj clean-all

clean-build:
	@echo ""
	@echo "********** REMOVING BUILD FILES **********"
	@echo ""
	rm -rf build/*
	@echo ""

clean-obj:
	@echo ""
	@echo "********** REMOVING OBJ FILES **********"
	@echo ""
	rm -rf obj/*
	@echo ""

clean-all: clean-obj clean-build
