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

# MinGW g++ uzantısız -o çıktısına .exe ekler; 'install' gerçek dosya adını arar.
ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPS    := $(OBJECTS:.o=.d)

# Gömme kütüphanesi: main.o hariç her şey. Host uygulaması (ve capi_test)
# bunu bağlar; main.o'daki main() ile çakışmasın diye ayrı tutuluyor.
LIB_OBJECTS := $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS))
LIB         := $(BUILD_DIR)/libraiden.a
CAPI_TEST   := $(BUILD_DIR)/capi_test

.PHONY: all lib run test install uninstall clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "--> $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

# --- WASM (Faz 4 / adım 2) ---
#
# emscripten ayrı bir araç zinciri; w64devkit'ten bağımsız. Kurulum:
#   C:\Users\imrai\tools\emsdk\emsdk_env.bat  (her oturumda, PATH'e emcc ekler)
#
# ALLOW_TABLE_GROWTH şart: JS tarafı host geri çağrısını addFunction ile
# tabloya ekliyor, o da tablonun büyüyebilmesini gerektiriyor.
#
# -fwasm-exceptions ŞART ve performans kararıdır. emscripten C++ istisnalarını
# varsayılan olarak KAPATIR; açık olmazsa ilk 'return' deyiminde __cxa_throw
# çağrılır ve modül abort eder. Yorumlayıcı akış kontrolünü (return/break/
# continue) ve betik istisnalarını C++ istisnasıyla taşıyor (interp.hpp).
# Alternatif -fexceptions (JS tabanlı) her yerde çalışır ama HER fonksiyon
# dönüşünde JS'e geçiş demek — bu mimaride kabul edilemez yavaş. Native wasm
# EH: Chrome 95+, Firefox 100+, Safari 15.2+, Node 18+.
EMCXX     ?= em++
WASM_DIR  := dist
WASM_OUT  := $(WASM_DIR)/raidenscript.js
WASM_SRC  := $(filter-out $(SRC_DIR)/main.cpp,$(SOURCES))
WASM_EXPORTS := '["_rs_new","_rs_free","_rs_set_host","_rs_register","_rs_eval","_rs_call","_rs_last_error","_malloc","_free"]'
WASM_RUNTIME := '["ccall","cwrap","addFunction","removeFunction","UTF8ToString","stringToUTF8","lengthBytesUTF8","getValue","setValue","HEAPF64"]'

.PHONY: wasm

wasm: $(WASM_OUT)

$(WASM_OUT): $(WASM_SRC)
	@mkdir -p $(WASM_DIR)
	$(EMCXX) -std=c++20 -O2 -fwasm-exceptions $(WASM_SRC) -o $@ \
	  -sMODULARIZE=1 -sEXPORT_NAME=createRaidenScript \
	  -sEXPORTED_FUNCTIONS=$(WASM_EXPORTS) \
	  -sEXPORTED_RUNTIME_METHODS=$(WASM_RUNTIME) \
	  -sALLOW_TABLE_GROWTH=1 -sALLOW_MEMORY_GROWTH=1 \
	  -sENVIRONMENT=web,worker,node -sEXPORT_ES6=0
	@echo "--> $@"

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

# Yorumlayıcıyı kabuktan erişilebilir yere kurar: "rai deneme.rai".
#
# Varsayılan hedef ~/.local/bin — Windows'ta bu klasör kullanıcı PATH'inde
# zaten var, yani kayıt defterine dokunmadan komut anında çalışır. Başka yer
# istenirse: make install PREFIX=/başka/yer
#
# İsim build/rs -> rai: uzantı .rai, komut rai, dil RaidenScript.
HOME_DIR := $(if $(HOME),$(HOME),$(USERPROFILE))
PREFIX   ?= $(HOME_DIR)/.local
BINDIR   ?= $(PREFIX)/bin

install: $(TARGET)
	@mkdir -p "$(BINDIR)"
	@cp $(TARGET)$(EXE) "$(BINDIR)/rai$(EXE)"
	@echo "--> $(BINDIR)/rai$(EXE)"
	@echo "    dene: rai --version   (yeni bir kabuk penceresi gerekebilir)"

uninstall:
	@rm -f "$(BINDIR)/rai$(EXE)"
	@echo "kaldırıldı: $(BINDIR)/rai$(EXE)"

clean:
	@rm -rf $(BUILD_DIR)
	@echo "temizlendi"
