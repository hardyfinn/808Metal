# Path to the Rack SDK (override on the command line: make RACK_DIR=...)
RACK_DIR ?= ../Rack-SDK

FLAGS +=
SOURCES += $(wildcard src/*.cpp)
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk

# Engine checks, no Rack SDK needed
test:
	$(CXX) -O2 -Wall -Wextra -std=c++17 -Isrc test/engine_test.cpp -o test/engine_test && test/engine_test
.PHONY: test
