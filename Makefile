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
  # NOT: -fsanitize=undefined burada KULLANILAMAZ — w64devkit libubsan göndermiyor,
  # bağlayıcı "cannot find -lubsan" der. Yerine libstdc++'ın kendi denetimli kipi:
  # kap sınırları, geçersiz iteratör ve ömür hatalarını çalışma zamanında yakalar,
  # ek bir çalışma zamanı kütüphanesi istemez. ABI'yi değiştirdiği için TÜM
  # derleme birimleri aynı bayrakla derlenmeli (DEBUG=1 zaten hepsini derliyor).
  CXXFLAGS += -O0 -g3 -D_GLIBCXX_DEBUG -D_GLIBCXX_ASSERTIONS -DRS_DEBUG
else
  CXXFLAGS += -O2 -DNDEBUG
endif

SRC_DIR   := src
BUILD_DIR := build
TARGET    := $(BUILD_DIR)/rs

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

# Gömme kütüphanesi: main.o hariç her şey. Host uygulaması (ve capi_test)
# bunu bağlar; main.o'daki main() ile çakışmasın diye ayrı tutuluyor.
LIB_OBJECTS := $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS))
LIB         := $(BUILD_DIR)/libraiden.a
CAPI_TEST   := $(BUILD_DIR)/capi_test

.PHONY: all lib run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "--> $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

lib: $(LIB)

$(LIB): $(LIB_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(AR) rcs $@ $(LIB_OBJECTS)
	@echo "--> $@"

$(CAPI_TEST): tests/capi_test.cpp $(LIB)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) tests/capi_test.cpp $(LIB) -o $@ $(LDFLAGS)
	@echo "--> $@"

run: $(TARGET)
	@./$(TARGET) run $(FILE)

# C API koşumu. Ömür hatalarını yakalamak için: make DEBUG=1 test
test: $(CAPI_TEST)
	@./$(CAPI_TEST)

clean:
	@rm -rf $(BUILD_DIR)
	@echo "temizlendi"
