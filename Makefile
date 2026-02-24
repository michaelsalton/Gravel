BUILD_DIR := build
BINARY    := $(BUILD_DIR)/bin/Gravel

.PHONY: all run build configure clean

all: run

run: build
	$(BINARY)

build:
	cmake --build $(BUILD_DIR) -- -j$(shell nproc)

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

clean:
	cmake --build $(BUILD_DIR) --target clean
