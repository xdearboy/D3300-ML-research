CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: all run test clean
all: build/d3300_sim
build/d3300_sim: core/dml.c core/dml.h modules/intervalometer.c modules/intervalometer.h sim/main.c
	mkdir -p build
	$(CC) $(CFLAGS) core/dml.c modules/intervalometer.c sim/main.c -o $@
run: all
	./build/d3300_sim
test: all
	./build/d3300_sim
	python3 -m unittest discover -s tests -v
clean:
	rm -rf build
