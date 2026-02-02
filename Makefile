PREFIX ?= /usr/local
DESTDIR ?=
BINDIR = $(PREFIX)/bin

CXX ?= g++
CXXFLAGS += -std=c++17 -O3 -march=native -DNDEBUG -DEIGEN_NO_DEBUG -ffast-math -flto -funroll-loops -fno-finite-math-only #-Wall -Wextra -fsanitize=address 

ifeq ($(DEBUG), 1)
    CXXFLAGS += -DDEBUG_MODE
endif

INCLUDES = -Iinclude

ifeq ($(origin TERMUX__PREFIX), environment)
    CXXFLAGS += --rtlib=compiler-rt
    INCLUDES += -I$(TERMUX__PREFIX)/include/eigen3
else
    INCLUDES += -I/usr/include/eigen3
endif

LIBS_SNDFILE = -lsndfile 
LIBS_SAMPLERATE = -lsamplerate
LIBS_JACK = -ljack
LIBS_PORTAUDIO = -lportaudio
LIBS_FFTW3 = -lfftw3
LV2_CXXFLAGS = -fPIC -shared
LV2_INCLUDES = $(shell pkg-config --cflags lv2 2>/dev/null || echo "")

.PHONY: all clean install uninstall

all: bin/spicepedal bin/spicepedal-stream bin/spicepedal-jack bin/spicepedal-plot

bin/%: | bin
	@echo "Building $@"

bin/spicepedal: src/spicepedal.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LIBS_SNDFILE) $(LIBS_SAMPLERATE) ${DEBUG}

bin/spicepedal-stream: src/spicepedal_stream.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LIBS_SNDFILE) $(LIBS_SAMPLERATE) $(LIBS_PORTAUDIO) ${DEBUG}

bin/spicepedal-jack: src/spicepedal_jack.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@  $(LIBS_SNDFILE) $(LIBS_SAMPLERATE) $(LIBS_JACK) ${DEBUG}

bin/spicepedal-plot: src/spicepedal_plot.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -o $@ $(LIBS_FFTW3) ${DEBUG}

bin:
	@mkdir -p bin

install: all
	@install -d $(DESTDIR)$(BINDIR)
	@install -m 755 bin/spicepedal $(DESTDIR)$(BINDIR)/
	@install -m 755 bin/spicepedal-stream $(DESTDIR)$(BINDIR)/
	@install -m 755 bin/spicepedal-jack $(DESTDIR)$(BINDIR)/
	@install -m 755 bin/spicepedal-plot $(DESTDIR)$(BINDIR)/
	@echo "Binaries installed to $(DESTDIR)$(BINDIR)"

uninstall:
	@rm -f $(DESTDIR)$(BINDIR)/spicepedal*
	@echo "Binaries removed from $(DESTDIR)$(BINDIR)"

clean:
	@rm -rf bin/
