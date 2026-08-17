CXX := g++
MKTRX := ./tools/mktrx
MKTRXSO := ./tools/mktrxso
PYTHON := python3

MANIFEST := programs.manifest
PROGRAMS_MK := programs.mk

CXXFLAGS := \
	-ffreestanding \
	-fno-stack-protector \
	-mno-red-zone \
	-nostdlib \
	-fno-exceptions \
	-fno-rtti \
	-fpie \
	-O0 \
	-m64 \
	-Iinclude \
	-Icommon

LIB_CXXFLAGS := \
	-ffreestanding \
	-fno-stack-protector \
	-mno-red-zone \
	-nostdlib \
	-fno-exceptions \
	-fno-rtti \
	-fPIC \
	-fvisibility=hidden \
	-O0 \
	-m64 \
	-Iinclude \
	-Icommon

LDFLAGS := \
	-T user.ld \
	-e _start \
	-pie \
	-nostdlib \
	-Wl,--gc-sections \
	-Wl,-no-dynamic-linker

LIB_LDFLAGS := \
	-shared \
	-nostdlib \
	-Wl,--gc-sections \
	-Wl,-no-dynamic-linker

LIBS := mathlib

COMMON_OBJS := \
	bin/SyscallWrappers.o \
	bin/crt0.o \
	bin/process.o \
	bin/trx.o \
	bin/trx_so.o

mathlib_SRCS := libs/mathlib.cpp

# Automatically regenerate programs.mk if needed
-include $(PROGRAMS_MK)

all: $(PROGRAMS:%=bin/%.trx) $(LIBS:%=bin/%.trso)

$(PROGRAMS_MK): $(MANIFEST) tools/genmake.py
	$(PYTHON) tools/genmake.py $(MANIFEST) > $@

bin/SyscallWrappers.o: common/SyscallWrappers.asm
	@mkdir -p $(dir $@)
	nasm -f elf64 -o $@ $<

bin/process.o: common/temrixstd/sys/process.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/crt0.o: common/temrixstd/crt0.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/trx.o: common/temrixstd/sys/trx.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

bin/trx_so.o: common/temrixstd/trx_so.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Generic C++ compilation
bin/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Generate link rules from programs.mk
define PROGRAM_template

$(1)_OBJS := $$(patsubst %.cpp,bin/%.o,$$($(1)_SRCS)) $(COMMON_OBJS)

bin/$(1).elf: $$($(1)_OBJS)
	$$(CXX) $$(CXXFLAGS) -o $$@ $$^ $$(LDFLAGS)

endef

$(foreach prog,$(PROGRAMS),$(eval $(call PROGRAM_template,$(prog))))

bin/%.trx: bin/%.elf
	$(MKTRX) $< $@

# separate object dir so -fPIC objects never collide with the -fpie ones
bin/lib/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(LIB_CXXFLAGS) -c -o $@ $<

define LIB_template

$(1)_LIBOBJS := $$(patsubst %.cpp,bin/lib/%.o,$$($(1)_SRCS))

bin/$(1).so: $$($(1)_LIBOBJS)
	$$(CXX) $$(LIB_CXXFLAGS) $$(LIB_LDFLAGS) -Wl,-soname,$(1).so -o $$@ $$^

endef

$(foreach lib,$(LIBS),$(eval $(call LIB_template,$(lib))))

bin/%.trso: bin/%.so
	$(MKTRXSO) $< $@

clean:
	rm -rf bin
	
.PHONY: all clean