CXX := clang++
CC := clang

ifeq ($(shell uname -s),Darwin)
SHLIB_SUFFIX := dylib
PRELOAD_VAR  := DYLD_INSERT_LIBRARIES
DARWIN_DYLIB_FLAGS := -shared -fPIC -compatibility_version 1 -current_version 1 \
                      -dynamiclib
CXXLIB       := $(CXX) $(DARWIN_DYLIB_FLAGS)
CCLIB        := $(CC) $(DARWIN_DYLIB_FLAGS)
else
SHLIB_SUFFIX := so
PRELOAD_VAR  := LD_PRELOAD
CXXLIB       := $(CXX) -shared -fPIC -Wl,-soname,interposer.so
CCLIB        := $(CC) -shared -fPIC
endif

all: logger.$(SHLIB_SUFFIX) logger_c.$(SHLIB_SUFFIX)

clean:
	@rm -f logger.$(SHLIB_SUFFIX) logger_c.$(SHLIB_SUFFIX) testprog

logger.$(SHLIB_SUFFIX): logger.cc ../../include/interpose.hh
	$(CXXLIB) -I../../include --std=c++17 -o $@ $< -ldl

logger_c.$(SHLIB_SUFFIX): logger.c ../../include/interpose.h
	$(CCLIB) -I../../include --std=c99 -o $@ $< -ldl

testprog: testprog.cc
	$(CXX) --std=c++17 -o $@ $<

test: testprog logger.$(SHLIB_SUFFIX)
	$(PRELOAD_VAR)=./logger.$(SHLIB_SUFFIX) ./testprog

test_c: testprog logger_c.$(SHLIB_SUFFIX)
	$(PRELOAD_VAR)=./logger_c.$(SHLIB_SUFFIX) ./testprog

.PHONY: all clean test
