CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_SOURCES = core/dml.c core/dml_menu.c modules/intervalometer.c
CORE_HEADERS = core/dml.h core/dml_menu.h modules/intervalometer.h

.PHONY: all run test clean
all: build/d3300_sim
build/d3300_sim: $(CORE_SOURCES) $(CORE_HEADERS) sim/main.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) sim/main.c -o $@
build/test_menu: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_menu.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_menu.c -o $@
run: all
	./build/d3300_sim
test: all build/test_menu
	./build/d3300_sim
	./build/test_menu
	python3 -m unittest discover -s tests -v
clean:
	rm -rf build
