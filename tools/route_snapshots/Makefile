CXX ?= clang++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic

CODEC_DIR := ../../src/networklayer/configurator/ipv4
TARGET := leo_route_snapshot_tool

.PHONY: all clean test

all: $(TARGET)

$(TARGET): leo_route_snapshot_tool.cc $(CODEC_DIR)/LeoRouteSnapshot.cc $(CODEC_DIR)/LeoRouteSnapshot.h
	$(CXX) $(CXXFLAGS) -I$(CODEC_DIR) leo_route_snapshot_tool.cc $(CODEC_DIR)/LeoRouteSnapshot.cc -o $@

test: all
	$(MAKE) -C ../../tests/route_snapshots test

clean:
	rm -f $(TARGET)
