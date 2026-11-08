# Re-run the Section 8 measurements against the standalone renderer; fails if any threshold regresses.
#   make -C measure verify                  (builds the renderer + unit tests with clang++ first)
#   make -C measure verify RENDERER=../build/sbf_render REFERENCE=../captures
PYTHON   ?= python3
CXX      ?= c++
BUILD    ?= ../build-measure
RENDERER ?= $(BUILD)/sbf_render
TESTS    ?= $(BUILD)/sbf_tests
OUT      ?= ../docs/results
REFERENCE ?=
REFARG   := $(if $(REFERENCE),--reference $(REFERENCE),)

.PHONY: verify renderer tests clean

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/sbf_render: ../tools/Render.cpp ../src/dsp/*.h | $(BUILD)
	$(CXX) -std=c++17 -O2 -Wall -Wextra ../tools/Render.cpp -o $@

$(BUILD)/sbf_tests: ../tests/UnitTests.cpp ../src/dsp/*.h | $(BUILD)
	$(CXX) -std=c++17 -O2 -Wall -Wextra ../tests/UnitTests.cpp -o $@

renderer: $(BUILD)/sbf_render
tests: $(BUILD)/sbf_tests
	$(TESTS)

verify: renderer tests
	$(PYTHON) run_all.py $(RENDERER) --out $(OUT) $(REFARG)

clean:
	rm -rf $(BUILD)
