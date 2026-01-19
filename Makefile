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
INSTALL_DIR = $(USER_LV2_DIR)/$(LV2_BUNDLE)

PLUGIN_SO = spicepedal.so
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

lv2-lowpass-rc:
	@$(MAKE) lv2-install \
		CIRCUIT="lowpass-rc" \
		CIRCUIT_FILE="circuits/filters/lowpass-rc.cir" \
		PLUGIN_URI="http://github.com/buzzobuono/spicepedal#lowpass-rc"

lv2-highpass-rc:
	@$(MAKE) lv2-install \
		CIRCUIT="highpass-rc" \
		CIRCUIT_FILE="circuits/filters/highpass-rc.cir" \
		PLUGIN_URI="http://github.com/buzzobuono/spicepedal#highpass-rc"

lv2-wolly-mammoth:
	@$(MAKE) lv2-install \
		CIRCUIT="wolly-mammoth" \
		CIRCUIT_FILE="circuits/fuzzes/wolly-mammoth-partial.cir" \
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

lv2: lv2-clean
	@echo "Compilazione LV2 (Circuit: $(CIRCUIT))"
	@echo 
	@mkdir -p lib
	$(CXX) $(CXXFLAGS) $(LV2_CXXFLAGS) $(INCLUDES) $(LV2_INCLUDES)		src/lv2_plugin.cpp -o lib/$(PLUGIN_SO) ${DEBUG} -DPLUGIN_URI='"$(PLUGIN_URI)"'

lv2-install: lv2
	@mkdir -p $(INSTALL_DIR)
	@cp lib/$(PLUGIN_SO) $(INSTALL_DIR)/
	@sed -e 's|@CIRCUIT@|$(CIRCUIT)|g' -e 's|@PLUGIN_URI@|$(PLUGIN_URI)|g' ttl/manifest.ttl > $(INSTALL_DIR)/manifest.ttl
	@sed -e 's|@CIRCUIT@|$(CIRCUIT)|g' -e 's|@PLUGIN_URI@|$(PLUGIN_URI)|g' ttl/spicepedal.ttl > $(INSTALL_DIR)/spicepedal.ttl
	@cp -r $(CIRCUIT_FILE) $(INSTALL_DIR)/circuit.cir
	@echo "✓ Plugin LV2 (Circuit: $(CIRCUIT)) installato in $(INSTALL_DIR)"
	@echo "Test with: jalv $(PLUGIN_URI)"

lv2-clean:
	@rm -f lib/$(PLUGIN_SO)

lv2-uninstall:
	rm -rf $(INSTALL_DIR)
