CXX = g++
ifeq ($(origin TERMUX__PREFIX), environment)
    CXXFLAGS = --rtlib=compiler-rt -std=c++17 -O3 -march=native -DNDEBUG -DEIGEN_NO_DEBUG -ffast-math -flto -funroll-loops -fno-finite-math-only
    INCLUDES = -I$(TERMUX__PREFIX)/include/eigen3 -Iinclude
else
    CXXFLAGS = -std=c++17 -O3 -march=native -DNDEBUG -DEIGEN_NO_DEBUG -ffast-math -flto -funroll-loops -fno-finite-math-only #-Wall -Wextra -fsanitize=address 
    INCLUDES = -I/usr/include/eigen3 -Iinclude
endif
LIBS_SNDFILE = -lsndfile -lsamplerate
LIBS_PORTAUDIO = -lportaudio
LIBS_FFTW3 = -lfftw3
LV2_CXXFLAGS = -fPIC -shared
LV2_INCLUDES = $(shell pkg-config --cflags lv2 2>/dev/null || echo "")

CIRCUIT = default
CIRCUIT_FILE = circuits/default.cir
PLUGIN_URI = http://github.com/buzzobuono/spicepedal\#default

LV2_BUNDLE = spicepedal.$(CIRCUIT).lv2
LV2_INSTALL_DIR = $(USER_LV2_DIR)/$(LV2_BUNDLE)
PLUGIN_SO = spicepedal-$(CIRCUIT).so
USER_LV2_DIR = $(HOME)/.lv2
BIN_INSTALL_DIR = $(HOME)/bin

all: spicepedal spicepedal-stream spicepedal-plot

spicepedal: clean_spicepedal create_bin_folder
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal.cpp -o bin/spicepedal $(LIBS_SNDFILE) ${DEBUG}

spicepedal-stream: clean_spicepedal-stream create_bin_folder
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal_stream.cpp -o bin/spicepedal-stream $(LIBS_SNDFILE) $(LIBS_PORTAUDIO) ${LIBS_FFTW3} ${DEBUG}

spicepedal-plot: clean_spicepedal-plot create_bin_folder
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal_plot.cpp -o bin/spicepedal-plot ${LIBS_FFTW3} ${DEBUG}

install: all
	@mkdir -p $(BIN_INSTALL_DIR)
	@cp bin/spicepedal $(BIN_INSTALL_DIR)/
	@cp bin/spicepedal-stream $(BIN_INSTALL_DIR)/
	@cp bin/spicepedal-plot $(BIN_INSTALL_DIR)/
	@chmod +x $(BIN_INSTALL_DIR)/spicepedal
	@chmod +x $(BIN_INSTALL_DIR)/spicepedal-stream
	@chmod +x $(BIN_INSTALL_DIR)/spicepedal-plot
	@echo "✓ Binaries installed to ~/bin"
	@echo "✓ Make sure ~/bin is in your PATH"

uninstall:
	@rm -f $(BIN_INSTALL_DIR)/spicepedal
	@rm -f $(BIN_INSTALL_DIR)/spicepedal-stream
	@rm -f $(BIN_INSTALL_DIR)/spicepedal-plot
	@echo "✓ Binaries removed from ~/bin"

clean_spicepedal:
	@rm -f bin/spicepedal

clean_spicepedal-stream:
	@rm -f bin/spicepedal-stream

clean_spicepedal-plot:
	@rm -f bin/spicepedal-plot

create_bin_folder:
	@mkdir -p bin/ lib/

lv2-all: lv2-clean lv2-bazz-fuss lv2-lowpass-rc lv2-highpass-rc lv2-wolly-mammoth lv2-tone-stack

lv2-bazz-fuss:
	@$(MAKE) lv2-install \
		CIRCUIT="bazz-fuss" \
		CIRCUIT_FILE="circuits/fuzzes/bazz-fuss.cir" \
		PLUGIN_URI="http://github.com/buzzobuono/spicepedal#bazz-fuss"

lv2-wolly-mammoth:
	@$(MAKE) lv2-install \
		CIRCUIT="wolly-mammoth" \
		CIRCUIT_FILE="circuits/fuzzes/wolly-mammoth.cir" \
		PLUGIN_URI="http://github.com/buzzobuono/spicepedal#wolly-mammoth"

lv2-tone-stack:
	@$(MAKE) lv2-install \
		CIRCUIT="tone-stack" \
		CIRCUIT_FILE="circuits/tones/fender-bassman-tone-stack.cir" \
		PLUGIN_URI="http://github.com/buzzobuono/spicepedal#tone-stack"

lv2-tremolo:
	@$(MAKE) lv2-install \
		CIRCUIT="tremolo" \
		CIRCUIT_FILE="circuits/tremolo.cir" \
		PLUGIN_URI="http://github.com/buzzobuono/spicepedal#tremolo"

lv2: lv2-clean lv2-uninstall
	@echo "Compilazione LV2 (Circuit: $(CIRCUIT))"
	@echo 
	@mkdir -p lib
	$(CXX) $(CXXFLAGS) $(LV2_CXXFLAGS) $(INCLUDES) $(LV2_INCLUDES)		src/lv2_plugin.cpp -o lib/$(PLUGIN_SO) ${DEBUG} -DPLUGIN_URI='"$(PLUGIN_URI)"'

lv2-install: $(LV2_INSTALL_DIR) lv2-ttl lv2
	@cp lib/$(PLUGIN_SO) $(LV2_INSTALL_DIR)/
	@cp -r $(CIRCUIT_FILE) $(LV2_INSTALL_DIR)/circuit.cir
	@echo "Plugin LV2 (Circuit: $(CIRCUIT)) installed in $(LV2_INSTALL_DIR)"
	@echo "Test with: jalv $(PLUGIN_URI)"

$(LV2_INSTALL_DIR):
	@mkdir -p $(LV2_INSTALL_DIR)

lv2-uninstall:
	@rm -rf $(LV2_INSTALL_DIR)

lv2-clean:
	@rm -f lib/$(PLUGIN_SO)

lv2-ttl: $(CIRCUIT_FILE) ttl/spicepedal-head.ttl
	@mkdir -p $(BIN_INSTALL_DIR)
	@sed -e 's|@CIRCUIT@|$(CIRCUIT)|g' -e 's|@PLUGIN_URI@|$(PLUGIN_URI)|g' -e 's|@PLUGIN_SO@|$(PLUGIN_SO)|g' ttl/manifest.ttl > $(LV2_INSTALL_DIR)/manifest.ttl
	@sed -e 's|@CIRCUIT@|$(CIRCUIT)|g' -e 's|@PLUGIN_URI@|$(PLUGIN_URI)|g' ttl/spicepedal-head.ttl > $(LV2_INSTALL_DIR)/spicepedal.ttl
	@grep "^\.ctrl" $(CIRCUIT_FILE) | awk '{ \
		idx=$$2; sym=$$3; min=$$4; max=$$5; \
		printf "    ,\n"; \
		printf "    [\n        a lv2:InputPort ,\n            lv2:ControlPort ;\n"; \
		printf "        lv2:index %d ;\n", idx + 4; \
		printf "        lv2:symbol \"%s\" ;\n", sym; \
		printf "        lv2:name \"%s\" ;\n", toupper(substr(sym,1,1)) substr(sym,2); \
		printf "        lv2:default %s ;\n", min; \
		printf "        lv2:minimum %s ;\n", min; \
		printf "        lv2:maximum %s ;\n", max; \
 printf "    ]\n"; \
	}' >> $(LV2_INSTALL_DIR)/spicepedal.ttl
	@echo "    ." >> $(LV2_INSTALL_DIR)/spicepedal.ttl