CXX ?= clang++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -Wpedantic

IGRAPH_CFLAGS := $(shell pkg-config --cflags igraph)
IGRAPH_LIBS := $(shell pkg-config --libs igraph)
SOURCE_DIR := ../../src/networklayer/configurator/ipv4
TARGET := KShortestPathTests
SOURCES := KShortestPathTests.cc \
	$(SOURCE_DIR)/LeoKShortestPaths.cc \
	$(SOURCE_DIR)/LeoKPathSnapshot.cc \
	$(SOURCE_DIR)/LeoRouteSnapshot.cc

.PHONY: all test clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(SOURCE_DIR)/LeoKShortestPaths.h $(SOURCE_DIR)/LeoKPathSnapshot.h $(SOURCE_DIR)/LeoRouteSnapshot.h
	$(CXX) $(CXXFLAGS) $(IGRAPH_CFLAGS) -I$(SOURCE_DIR) $(SOURCES) $(IGRAPH_LIBS) -o $@

test: all
	./$(TARGET)

clean:
	rm -f $(TARGET)
