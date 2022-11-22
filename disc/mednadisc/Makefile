ROOT_DIR := $(realpath .)
OUTPUT_DIR := $(realpath $(ROOT_DIR)/../../output)

OUT_DIR := $(ROOT_DIR)/obj
OBJ_DIR := $(OUT_DIR)/release
DOBJ_DIR := $(OUT_DIR)/debug

MEDNAFLAGS := -I$(ROOT_DIR) -I$(ROOT_DIR)/trio -fwrapv -fno-strict-aliasing -fsigned-char \
	-fno-aggressive-loop-optimizations -fno-fast-math -fno-unsafe-math-optimizations -fjump-tables -fPIC \
	-Wall -Wempty-body -Wvla -Wvariadic-macros -Wdisabled-optimization -Wno-shadow -Wno-write-strings -Wno-unused-variable \
	-Wno-ignored-qualifiers -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-const-variable -Wno-uninitialized

CC := gcc
CXX := g++
CCFLAGS := $(MEDNAFLAGS) -std=gnu99
CXXFLAGS := $(MEDNAFLAGS) -std=gnu++11

TARGET := libmednadisc.so

SRCS := \
	$(ROOT_DIR)/cdrom/CDAccess.cpp \
	$(ROOT_DIR)/cdrom/CDAccess_CCD.cpp \
	$(ROOT_DIR)/cdrom/CDAccess_Image.cpp \
	$(ROOT_DIR)/cdrom/CDAFReader.cpp \
	$(ROOT_DIR)/cdrom/cdromif.cpp \
	$(ROOT_DIR)/cdrom/CDUtility.cpp \
	$(ROOT_DIR)/cdrom/crc32.cpp \
	$(ROOT_DIR)/cdrom/galois.cpp \
	$(ROOT_DIR)/cdrom/l-ec.cpp \
	$(ROOT_DIR)/cdrom/lec.cpp \
	$(ROOT_DIR)/cdrom/recover-raw.cpp \
	$(ROOT_DIR)/error.cpp \
	$(ROOT_DIR)/FileStream.cpp \
	$(ROOT_DIR)/general.cpp \
	$(ROOT_DIR)/mdfn_endian.cpp \
	$(ROOT_DIR)/Mednadisc.cpp \
	$(ROOT_DIR)/MemoryStream.cpp \
	$(ROOT_DIR)/stream.cpp \
	$(ROOT_DIR)/string/trim.cpp \
	$(ROOT_DIR)/trio/trio.c \
	$(ROOT_DIR)/trio/trionan.c \
	$(ROOT_DIR)/trio/triostr.c

LDFLAGS := -shared
CCFLAGS_DEBUG := -O0 -g -DMDFN_DEBUG
CCFLAGS_RELEASE := -O3 -flto
CXXFLAGS_DEBUG := -O0 -g
CXXFLAGS_RELEASE := -O3 -flto
LDFLAGS_DEBUG :=
LDFLAGS_RELEASE := -s

_OBJS := $(addsuffix .o,$(realpath $(SRCS)))
OBJS := $(patsubst $(ROOT_DIR)%,$(OBJ_DIR)%,$(_OBJS))
DOBJS := $(patsubst $(ROOT_DIR)%,$(DOBJ_DIR)%,$(_OBJS))

$(OBJ_DIR)/%.c.o: %.c
	@echo cc $<
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CCFLAGS) $(CCFLAGS_RELEASE)
$(OBJ_DIR)/%.cpp.o: %.cpp
	@echo cxx $<
	@mkdir -p $(@D)
	@$(CXX) -c -o $@ $< $(CXXFLAGS) $(CXXFLAGS_RELEASE)
$(DOBJ_DIR)/%.c.o: %.c
	@echo cc $<
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CCFLAGS) $(CCFLAGS_DEBUG)
$(DOBJ_DIR)/%.cpp.o: %.cpp
	@echo cxx $<
	@mkdir -p $(@D)
	@$(CXX) -c -o $@ $< $(CXXFLAGS) $(CXXFLAGS_DEBUG)

.DEFAULT_GOAL := install

TARGET_RELEASE := $(OBJ_DIR)/$(TARGET)
TARGET_DEBUG := $(DOBJ_DIR)/$(TARGET)

.PHONY: release debug install install-debug

release: $(TARGET_RELEASE)
debug: $(TARGET_DEBUG)

$(TARGET_RELEASE): $(OBJS)
	@echo ld $@
	@$(CXX) -o $@ $(LDFLAGS) $(LDFLAGS_RELEASE) $(CXXFLAGS) $(CXXFLAGS_RELEASE) $(OBJS)
$(TARGET_DEBUG): $(DOBJS)
	@echo ld $@
	@$(CXX) -o $@ $(LDFLAGS) $(LDFLAGS_DEBUG) $(CXXFLAGS) $(CXXFLAGS_DEBUG) $(DOBJS)

install: $(TARGET_RELEASE)
	@cp -f $< $(OUTPUT_DIR)
	@echo Release build of $(TARGET) installed.

install-debug: $(TARGET_DEBUG)
	@cp -f $< $(OUTPUT_DIR)
	@echo Debug build of $(TARGET) installed.

.PHONY: clean clean-release clean-debug
clean:
	rm -rf $(OUT_DIR)
clean-release:
	rm -rf $(OUT_DIR)/release
clean-debug:
	rm -rf $(OUT_DIR)/debug

-include $(OBJS:%o=%d)
-include $(DOBJS:%o=%d)
