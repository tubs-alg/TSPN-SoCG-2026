# Makefile for building and testing the TSPN project.
#
# Targets:
#   install-python  — install the Python package (triggers C++ build via skbuild-conan)
#   build-cpp       — configure and build the C++ library and tests
#   test-cpp        — run C++ tests (Google Test)
#   test-python     — run Python tests with pytest
#   test            — run both C++ and Python tests
#   lint            — run ruff linter and formatter
#   clean           — remove build artifacts

.PHONY: install-python build-cpp test-cpp test-python test lint clean

# Directories
BUILD_DIR     := build
CONAN_DIR     := .conan/release
TOOLCHAIN     := $(CONAN_DIR)/conan_toolchain.cmake
TEST_BIN      := $(BUILD_DIR)/tests/cpp/cpp_tests

# Prefer Ninja if available
CMAKE_GENERATOR := $(shell command -v ninja >/dev/null 2>&1 && echo "Ninja" || echo "Unix Makefiles")

# ── Python (via skbuild-conan, handles Conan internally) ─────────────
install-python:
	pip install .

# ── C++ build ────────────────────────────────────────────────────────
$(TOOLCHAIN):
	conan install . --output-folder=$(CONAN_DIR) --build=missing

build-cpp: $(TOOLCHAIN)
	cmake -S . -B $(BUILD_DIR) \
		-G "$(CMAKE_GENERATOR)" \
		-DCMAKE_TOOLCHAIN_FILE=$(TOOLCHAIN) \
		-DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) --target cpp_tests -j4

# ── Tests ────────────────────────────────────────────────────────────
test-cpp: build-cpp
	$(TEST_BIN)

test-python: install-python
	pytest -s tests/python/

test: test-cpp test-python

# ── Linting ──────────────────────────────────────────────────────────
lint:
	ruff check --fix .
	ruff format .

# ── Cleanup ──────────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR) $(CONAN_DIR) _skbuild
