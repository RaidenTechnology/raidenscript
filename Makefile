# RaidenScript — build
#
# Kullanım:
#   make            → build/rs.exe üretir
#   make run FILE=examples/01-temeller.rai
#   make test
#   make clean
#
# Araç zincirini kendisi bulur; sistem PATH'ine dokunmaya gerek yok.

CXX      ?= g++
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
LDFLAGS  :=

ifeq ($(DEBUG),1)
  CXXFLAGS += -O0 -g3 -fsanitize=undefined -DRS_DEBUG
else
  CXXFLAGS += -O2 -DNDEBUG
endif

SRC_DIR   := src
BUILD_DIR := build
TARGET    := $(BUILD_DIR)/rs

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "--> $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: $(TARGET)
	@./$(TARGET) run $(FILE)

test: $(TARGET)
	@./tests/run.sh

clean:
	@rm -rf $(BUILD_DIR)
	@echo "temizlendi"
