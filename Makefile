# Shortcuts for the Docker build environment, and for building natively when Qt is
# already installed.
#
# `make` on its own prints what is available.
#
# The docker targets need nothing but Docker: the image carries the compiler, Qt and
# every library the app links against. The native- targets need Qt 6.5+ and Ninja on the
# machine, and are what the maintainer uses day to day.

COMPOSE ?= docker compose
RUN     := $(COMPOSE) run --rm

# Kept apart from build/, which a native build on the host already owns.
NATIVE_BUILD_DIR ?= build

.DEFAULT_GOAL := help
.PHONY: help image test smoke build run shell clean rebuild native-build native-test native-run native-clean

help:
	@echo "Pomodoro - development commands"
	@echo
	@echo "  In Docker (nothing to install but Docker itself):"
	@echo "    make image         build the toolchain image"
	@echo "    make test          compile and run the tests"
	@echo "    make smoke         start the app headless, check its QML and icons load"
	@echo "    make build         compile only"
	@echo "    make run           open the window (needs an X server, see README)"
	@echo "    make shell         a prompt inside the toolchain"
	@echo "    make clean         drop the container build tree"
	@echo "    make rebuild       rebuild the image from scratch, then test"
	@echo
	@echo "  Natively (needs Qt 6.5+, CMake 3.21+ and Ninja):"
	@echo "    make native-build  configure and compile into $(NATIVE_BUILD_DIR)/"
	@echo "    make native-test   the same, then run ctest"
	@echo "    make native-run    build and launch"
	@echo "    make native-clean  delete $(NATIVE_BUILD_DIR)/"

# --- Docker -----------------------------------------------------------------

image:
	$(COMPOSE) build

test:
	$(RUN) test

smoke:
	$(RUN) smoke

build:
	$(RUN) build

run:
	$(RUN) app

shell:
	$(RUN) shell

clean:
	-$(RUN) test clean
	-$(COMPOSE) down --volumes

# For when the dependency list itself changed and the cached apt layer is stale.
rebuild:
	$(COMPOSE) build --no-cache
	$(RUN) test

# --- Native -----------------------------------------------------------------

native-build:
	cmake -S . -B $(NATIVE_BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
	cmake --build $(NATIVE_BUILD_DIR) --parallel

native-test: native-build
	ctest --test-dir $(NATIVE_BUILD_DIR) --output-on-failure

native-run: native-build
	$(NATIVE_BUILD_DIR)/pomodoro

native-clean:
	rm -rf $(NATIVE_BUILD_DIR)
