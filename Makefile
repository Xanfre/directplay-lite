# DirectPlay Lite

objdir      = ./objs
srcobjdir   = ./objs/dpnet
testsobjdir = ./objs/tests

PREFIX ?= /usr
LIBDIR = $(PREFIX)/bin
LICDIR = $(PREFIX)/share/licenses/libdpnet

TARGET ?=
TARGET_OS ?= $(OS)

ifeq ($(TARGET_OS),Windows_NT)
SONAME ?= dpnet.dll
else
SONAME ?= libdpnet.so.0
endif

ifeq ($(TARGET),)
CXX ?= g++
else
CXX = $(TARGET)-g++
endif

ifeq ($(DEBUG),)
OPTIMIZATION = -O2
else
OPTIMIZATION = -g -O0
endif

ifeq ($(TARGET_OS),Windows_NT)
CXXFLAGS_ALL = -std=c++17 $(OPTIMIZATION) -Wall -Iinclude $(CXXFLAGS)
TEST_CXXFLAGS_ALL = -std=c++17 $(OPTIMIZATION) -Wall -Iinclude -Itests $(CXXFLAGS)
LDFLAGS_ALL = -shared $(LDFLAGS)
TEST_LDFLAGS_ALL = $(LDFLAGS)
LIBS = -ldxguid -liphlpapi -lws2_32 -lole32 -luuid
TEST_LIBS = -lgtest
else
CXXFLAGS_ALL = -std=c++17 $(OPTIMIZATION) -Wall -fPIC -fvisibility=hidden -include include/unix/windows.h -Iinclude -Iinclude/unix $(CXXFLAGS)
TEST_CXXFLAGS_ALL = -std=c++17 $(OPTIMIZATION) -Wall -include include/unix/windows.h -Iinclude -Iinclude/unix -Itests $(CXXFLAGS)
LDFLAGS_ALL = $(OPTIMIZATION) -shared -Wl,-soname,$(SONAME) -Wl,--no-undefined $(LDFLAGS)
TEST_LDFLAGS_ALL = $(OPTIMIZATION) $(LDFLAGS)
LIBS = -lpthread
TEST_LIBS = -lgtest
endif

ifeq ($(DEBUG),)
LDFLAGS_ALL += -s
TEST_LDFLAGS_ALL += -s
endif

DPNET_OBJS = $(srcobjdir)/AsyncHandleAllocator.o \
			$(srcobjdir)/COMAPIException.o \
			$(srcobjdir)/DirectPlay8Address.o \
			$(srcobjdir)/DirectPlay8Peer.o \
			$(srcobjdir)/EventObject.o \
			$(srcobjdir)/HandleHandlingPool.o \
			$(srcobjdir)/HostEnumerator.o \
			$(srcobjdir)/Log.o \
			$(srcobjdir)/network.o \
			$(srcobjdir)/packet.o \
			$(srcobjdir)/SendQueue.o

ifeq ($(TARGET_OS),Windows_NT)
DPNET_OBJS += $(srcobjdir)/dpnet.o
DPNET_DEF = ./src/dpnet.def
else
DPNET_OBJS += $(srcobjdir)/dpnet_unx.o
DPNET_DEF =
endif

TEST_OBJS = $(testsobjdir)/DirectPlay8Address.o \
			$(testsobjdir)/DirectPlay8Peer.o \
			$(testsobjdir)/HandleHandlingPool.o \
			$(testsobjdir)/PacketDeserialiser.o \
			$(testsobjdir)/PacketSerialiser.o \
			$(testsobjdir)/SendQueue.o \
			$(testsobjdir)/tests_main.o

.PHONY: all install clean tests

all: $(SONAME)

install: $(SONAME)
	mkdir -p $(DESTDIR)$(LIBDIR)
	mkdir -p $(DESTDIR)$(LICDIR)
	install -p -m 755 $(SONAME) $(DESTDIR)$(LIBDIR)
	install -p -m 644 LICENSE.txt $(DESTDIR)$(LICDIR)

clean:
	rm -rf $(objdir)
	rm -f $(SONAME) test_dpnet

$(srcobjdir):
	mkdir -p $@

$(testsobjdir):
	mkdir -p $@

$(srcobjdir)/%.o: ./src/%.cpp | $(srcobjdir)
	$(CXX) $(CXXFLAGS_ALL) -c -o $@ $<

$(testsobjdir)/%.o: ./tests/%.cpp | $(testsobjdir)
	$(CXX) $(TEST_CXXFLAGS_ALL) -c -o $@ $<

$(SONAME): $(DPNET_OBJS) $(DPNET_DEF)
	$(CXX) $(LDFLAGS_ALL) -o $@ $^ $(LIBS)

tests: $(DPNET_OBJS) $(TEST_OBJS)
	$(CXX) $(TEST_LDFLAGS_ALL) -o test_dpnet  $^ $(LIBS) $(TEST_LIBS)

