ROOT_DIR := $(realpath .)
OUTPUT_DIR := $(realpath $(ROOT_DIR)/output)

OUT_DIR := $(ROOT_DIR)/obj
OBJ_DIR := $(OUT_DIR)/release
DOBJ_DIR := $(OUT_DIR)/debug

CC := gcc
CCFLAGS := -I$(ROOT_DIR)/common -I$(ROOT_DIR)/core -I$(ROOT_DIR)/disc -I$(ROOT_DIR)/encoding -I$(ROOT_DIR)/gpgx -I$(ROOT_DIR)/wbx \
	-Wall -Wextra -std=c11 -fno-strict-aliasing

TARGET := slimhawk

SRCS := \
	$(ROOT_DIR)/common/alloc.c \
	$(ROOT_DIR)/common/file.c \
	$(ROOT_DIR)/common/stub.c \
	$(ROOT_DIR)/core/core.c \
	$(ROOT_DIR)/disc/disc_impl.c \
	$(ROOT_DIR)/gpgx/gpgx_api.c \
	$(ROOT_DIR)/gpgx/gpgx_impl.c \
	$(ROOT_DIR)/encoding/encoding_impl.c \
	$(ROOT_DIR)/wbx/wbx_api.c \
	$(ROOT_DIR)/wbx/wbx_impl.c

LIBS := -L $(OUTPUT_DIR) -lwaterboxhost -lmednadisc -lavcodec -lavformat -lavutil -lswscale
LDFLAGS := -Wl,-R. -pthread
CCFLAGS_DEBUG := -O0 -g
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
$(DOBJ_DIR)/%.c.o: %.c
	@echo cc $<
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CCFLAGS) $(CCFLAGS_DEBUG)

.DEFAULT_GOAL := install

TARGET_RELEASE := $(OBJ_DIR)/$(TARGET)
TARGET_DEBUG := $(DOBJ_DIR)/$(TARGET)

.PHONY: release debug install install-debug

release: $(TARGET_RELEASE)
debug: $(TARGET_DEBUG)

$(TARGET_RELEASE): $(OBJS)
	@echo ld $@
	@$(CC) -o $@ $(LDFLAGS) $(LDFLAGS_RELEASE) $(CCFLAGS) $(CCFLAGS_RELEASE) $(OBJS) $(LIBS)
$(TARGET_DEBUG): $(DOBJS)
	@echo ld $@
	@$(CC) -o $@ $(LDFLAGS) $(LDFLAGS_DEBUG) $(CCFLAGS) $(CCFLAGS_DEBUG) $(DOBJS) $(LIBS)

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
