CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2
LDFLAGS ?= -lm

CORE_SOURCES = core/dml.c core/dml_menu.c core/dml_nikon_adapter.c core/dml_nikon_menu_contract.c core/dml_nikon_relay_snapshot.c core/dml_ui_logical_event.c core/dml_draw_buffer.c core/dml_fonts_data.c core/dml_tkernel.c core/dml_camera_status.c core/dml_camera_status_hw.c core/dml_info_screen.c core/dml_menu_screen.c payload/dml_payload.c modules/intervalometer.c modules/fuji_film_sim.c modules/photo_assist.c
CORE_HEADERS = core/dml.h core/dml_menu.h core/dml_nikon_adapter.h core/dml_nikon_menu_contract.h core/dml_nikon_relay_snapshot.h core/dml_ui_logical_event.h core/dml_draw_buffer.h core/dml_fonts_data.h core/dml_tkernel.h core/dml_camera_status.h core/dml_camera_status_hw.h core/dml_info_screen.h core/dml_menu_screen.h payload/dml_payload.h modules/intervalometer.h modules/fuji_film_sim.h modules/photo_assist.h

.PHONY: all run test audit-fast clean
all: build/d3300_sim
build/d3300_sim: $(CORE_SOURCES) $(CORE_HEADERS) sim/main.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) sim/main.c $(LDFLAGS) -o $@
build/test_menu: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_menu.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_menu.c $(LDFLAGS) -o $@
build/test_adapter: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_adapter.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_adapter.c $(LDFLAGS) -o $@
build/test_draw_buffer: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_draw_buffer.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_draw_buffer.c $(LDFLAGS) -o $@
build/test_tkernel: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_tkernel.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_tkernel.c $(LDFLAGS) -o $@
build/test_payload: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_payload.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_payload.c $(LDFLAGS) -o $@
build/test_info_screen: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_info_screen.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_info_screen.c $(LDFLAGS) -o $@
build/test_camera_status_hw: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_camera_status_hw.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_camera_status_hw.c $(LDFLAGS) -o $@
build/test_menu_screen: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_menu_screen.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_menu_screen.c $(LDFLAGS) -o $@
build/test_fuji_film_sim: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_fuji_film_sim.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_fuji_film_sim.c $(LDFLAGS) -o $@
build/test_photo_assist: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_photo_assist.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_photo_assist.c $(LDFLAGS) -o $@
build/test_mvp_pipeline: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_mvp_pipeline.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_mvp_pipeline.c $(LDFLAGS) -o $@
build/test_relay_snapshot: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_relay_snapshot.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_relay_snapshot.c $(LDFLAGS) -o $@
build/test_ui_logical_event: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_ui_logical_event.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_ui_logical_event.c $(LDFLAGS) -o $@
build/test_nikon_menu_contract: $(CORE_SOURCES) $(CORE_HEADERS) tests/test_nikon_menu_contract.c
	mkdir -p build
	$(CC) $(CFLAGS) $(CORE_SOURCES) tests/test_nikon_menu_contract.c $(LDFLAGS) -o $@
run: all
	./build/d3300_sim
test: all build/test_menu build/test_adapter build/test_draw_buffer build/test_tkernel build/test_payload build/test_info_screen build/test_camera_status_hw build/test_menu_screen build/test_fuji_film_sim build/test_photo_assist build/test_mvp_pipeline build/test_relay_snapshot build/test_ui_logical_event build/test_nikon_menu_contract
	./build/d3300_sim
	./build/test_menu
	./build/test_adapter
	./build/test_draw_buffer
	./build/test_tkernel
	./build/test_payload
	./build/test_info_screen
	./build/test_camera_status_hw
	./build/test_menu_screen
	./build/test_fuji_film_sim
	./build/test_photo_assist
	./build/test_mvp_pipeline
	./build/test_relay_snapshot
	./build/test_ui_logical_event
	./build/test_nikon_menu_contract
	python3 -m unittest discover -s tests -v
audit-fast:
	@test -n "$(D3300_FIRMWARE)" || (echo "D3300_FIRMWARE=/path/to/b860_102.bin is required" >&2; exit 2)
	PYTHONPATH=tools python3 tools/d3300_fast_audit.py "$(D3300_FIRMWARE)"
	D3300_FIRMWARE="$(D3300_FIRMWARE)" python3 -m unittest -q tests.test_input_chain tests.test_registration_callsites tests.test_persistent_dispatcher
clean:
	rm -rf build
