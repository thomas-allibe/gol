CC=gcc
CFLAGS=
INCFLAGS=
EXEC=bonk

all: start build/$(EXEC)
	@echo ""
	@echo "********** COMPILATION DONE **********"
	@echo ""

build/$(EXEC): src/main.c
	$(CC) -o $@ $<

start:
	@echo ""
	@echo "********** COMPILATION START *********"
	@echo ""

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
