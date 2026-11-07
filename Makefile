.PHONY: all configure build clean run
BUILD_DIR ?= build

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release

build: configure
	cmake --build $(BUILD_DIR) --parallel

run: build
	./$(BUILD_DIR)/minicpp examples/demo.cpp --run

clean:
	rm -rf $(BUILD_DIR)
