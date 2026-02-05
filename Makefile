PREFIX ?= /usr/local
DESTDIR ?=
BINDIR = $(PREFIX)/bin
FAST_MATH ?= 1
DEBUG ?= 0

CXX ?= g++
CXXFLAGS += -std=c++17 -O3 -march=native -flto -funroll-loops -fno-math-errno -fno-trapping-math #-Wall -Wextra -fsanitize=address

ifeq ($(FAST_MATH), 1)
    CXXFLAGS += -ffast-math -freciprocal-math -fassociative-math -fno-signed-zeros -mprefer-vector-width=256
endif

ifeq ($(DEBUG), 1)
    CXXFLAGS += -DDEBUG_MODE
else
    CXXFLAGS += -DNDEBUG -DEIGEN_NO_DEBUG 
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

bin/spicepedal: src/spicepedal.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal.cpp -o bin/spicepedal $(LIBS_SNDFILE) $(LIBS_SAMPLERATE)

bin/spicepedal-stream: src/spicepedal_stream.cpp | bin
		$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal_stream.cpp -o bin/spicepedal-stream $(LIBS_SNDFILE) $(LIBS_SAMPLERATE) $(LIBS_PORTAUDIO)

bin/spicepedal-jack: src/spicepedal_jack.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal_jack.cpp -o bin/spicepedal-jack  $(LIBS_SNDFILE) $(LIBS_SAMPLERATE) $(LIBS_JACK)

bin/spicepedal-plot: src/spicepedal_plot.cpp | bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/spicepedal_plot.cpp -o bin/spicepedal-plot $(LIBS_FFTW3)

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
