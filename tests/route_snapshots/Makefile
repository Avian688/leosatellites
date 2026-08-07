CXX ?= clang++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic

CODEC_DIR := ../../src/networklayer/configurator/ipv4
TARGET := RouteSnapshotTests

.PHONY: all test test-real clean

all: $(TARGET)

$(TARGET): RouteSnapshotTests.cc $(CODEC_DIR)/LeoRouteSnapshot.cc $(CODEC_DIR)/LeoRouteSnapshot.h
	$(CXX) $(CXXFLAGS) -I$(CODEC_DIR) RouteSnapshotTests.cc $(CODEC_DIR)/LeoRouteSnapshot.cc -o $@

test: all
	./$(TARGET)

test-real: all
	@test -n "$(CORPUS)" || (echo "Set CORPUS=/path/to/v2/constellation" && exit 2)
	./$(TARGET) "$(CORPUS)"

clean:
	rm -f $(TARGET)
