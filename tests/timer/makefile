include prorab.mk



this_name := tests


#compiler flags
this_cxxflags += -std=c++11
this_cxxflags += -Wall
this_cxxflags += -DDEBUG
this_cxxflags += -fstrict-aliasing #strict aliasing!!!
this_cxxflags += -O3

this_srcs += main.cpp tests.cpp

this_ldlibs += -laika -lnitki

ifeq ($(prorab_os),macosx)
    this_cxxflags += -stdlib=libc++ #this is needed to be able to use c++11 std lib
    this_ldlibs += -lc++
else ifeq ($(prorab_os),windows)
    this_cxxflags += -I/usr/include -I/usr/local/include
    this_ldflags += -L/usr/lib -L/usr/local/lib
else
    this_ldlibs += -lpthread
endif

this_ldflags += -L$(prorab_this_dir)../../src/

this_ldlibs += -lstdc++

#add dependency on libaika.so
$(abspath $(prorab_this_dir)tests): $(abspath $(prorab_this_dir)../../src/libaika$(prorab_lib_extension))


$(eval $(prorab-build-app))

include $(prorab_this_dir)../test_target.mk


#include makefile for building aika
$(eval $(call prorab-include,$(prorab_this_dir)../../src/makefile))
