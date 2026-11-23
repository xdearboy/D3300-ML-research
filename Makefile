CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

CORE_SOURCES = core/dml.c core/dml_menu.c core/dml_nikon_adapter.c modules/intervalometer.c
CORE_HEADERS = core/dml.h core/dml_menu.h core/dml_nikon_adapter.h modules/intervalometer.h

.PHONY: all run test clean
all: build/d3300_sim
build/d3300_sim: $(CORE_SOURCES) $(CORE_HEADERS) sim/main.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) sim/main.c -o $@
build/test_menu: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_menu.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_menu.c -o $@
build/test_adapter: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_adapter.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_adapter.c -o $@
run: all
	./build/d3300_sim
test: all build/test_menu build/test_adapter
	./build/d3300_sim
	./build/test_menu
	./build/test_adapter
	python3 -m unittest discover -s tests -v
clean:
	rm -rf build
