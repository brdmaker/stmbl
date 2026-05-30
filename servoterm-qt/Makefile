# Convenience wrapper so you can just type "make" or "make launch".
BUILD ?= build
CMAKE_PREFIX ?= /usr/lib/x86_64-linux-gnu/cmake

.PHONY: all build launch clean

all: build

build:
	cmake -S . -B $(BUILD) -DCMAKE_BUILD_TYPE=Release \
	      -DCMAKE_PREFIX_PATH=$(CMAKE_PREFIX)
	cmake --build $(BUILD) -j$$(nproc)

launch: build
	@$(MAKE) -C ../host stmbl_host --no-print-directory
	$(BUILD)/servoterm --sim ../host/stmbl_host

clean:
	rm -rf $(BUILD)
