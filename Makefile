PREFIX ?= /usr/local
DESTDIR ?=
BINDIR = $(PREFIX)/bin

FAST_MATH ?= 1
DEBUG ?= 0
BACKEND ?= INTERNAL

CXX ?= g++

CXXFLAGS += -std=c++17 -O3 -march=native -flto -funroll-loops -fno-math-errno -fno-trapping-math #-Wall -Wextra -fsanitize=address
ifeq ($(FAST_MATH), 1)
    CXXFLAGS += -ffast-math -freciprocal-math -fassociative-math -fno-signed-zeros -mprefer-vector-width=256
endif
ifeq ($(origin TERMUX__PREFIX), environment)
    CXXFLAGS += --rtlib=compiler-rt
endif

CPPFLAGS += -Iinclude
ifeq ($(DEBUG), 1)
    CPPFLAGS += -DDEBUG_MODE
else
    CPPFLAGS += -DNDEBUG -DEIGEN_NO_DEBUG 
endif
ifeq ($(BACKEND), INTERNAL)
    CPPFLAGS += -DBACKEND_INTERNAL
else
    CPPFLAGS += $(shell pkg-config --cflags eigen3)
endif

LDFLAGS +=

LDLIBS +=

.PHONY: all clean install uninstall

all: bin/spicepedal bin/spicepedal-stream bin/spicepedal-jack bin/spicepedal-plot

bin/spicepedal: LDLIBS += $(shell pkg-config --libs sndfile samplerate)
bin/spicepedal: CPPFLAGS += $(shell pkg-config --cflags sndfile samplerate)
bin/spicepedal: src/spicepedal.cpp | bin
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

bin/spicepedal-stream: LDLIBS += $(shell pkg-config --libs portaudio-2.0 sndfile samplerate)
bin/spicepedal-stream: CPPFLAGS += $(shell pkg-config --cflags portaudio-2.0 sndfile samplerate)
bin/spicepedal-stream: src/spicepedal_stream.cpp | bin
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

bin/spicepedal-jack: LDLIBS += $(shell pkg-config --libs jack sndfile samplerate)
bin/spicepedal-jack: CPPFLAGS += $(shell pkg-config --cflags jack sndfile samplerate)
bin/spicepedal-jack: src/spicepedal_jack.cpp | bin
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

bin/spicepedal-plot: LDLIBS += $(shell pkg-config --libs fftw3)
bin/spicepedal-plot: CPPFLAGS += $(shell pkg-config --cflags fftw3)
bin/spicepedal-plot: src/spicepedal_plot.cpp | bin
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

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
