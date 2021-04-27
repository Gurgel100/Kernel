
CC = $(CROSSTOOL_PREFIX)gcc
LD = $(CROSSTOOL_PREFIX)gcc
AS = $(CROSSTOOL_PREFIX)as
AR = $(CROSSTOOL_PREFIX)ar
RM = rm -rf

OUTPUT_DIR = build

CPPFLAGS = -I"../include"
CFLAGS = -gdwarf-4 -Wall -Wextra -fmessage-length=0 -m64 -ffreestanding -fno-stack-protector -fno-omit-frame-pointer -fPIC -std=gnu99
ARFLAGS = -rs

C_SRCS = $(shell find -name '*.c')
S_SRCS = $(shell find -name '*.S' ! -name 'start.S')

C_OBJS = $(patsubst ./%,$(OUTPUT_DIR)/%,$(C_SRCS:.c=.o))
S_OBJS = $(patsubst ./%,$(OUTPUT_DIR)/%,$(S_SRCS:.S=.o))
OBJS := $(C_OBJS) $(S_OBJS)
DEPS := $(OBJS:.o=.d)

ifeq ($(BUILD_CONFIG), release)
	CFLAGS+= -O3
else
	CFLAGS+= -Og
endif

.PHONY: all
all: $(OUTPUT_DIR)/crt0.o libc

.PHONY: release
release:
	$(MAKE) BUILD_CONFIG=$@

.PHONY: install
install: all install-headers check-sysroot
	cp $(OUTPUT_DIR)/libc.a $(SYSROOT_DIR)/lib/libc.a
	cp $(OUTPUT_DIR)/crt0.o $(SYSROOT_DIR)/lib/crt0.o

.PHONY: install-headers
install-headers: check-sysroot
	cp -r ../include/* $(SYSROOT_DIR)/include/

.PHONY: check-sysroot
check-sysroot:
ifndef SYSROOT_DIR
	$(error SYSROOT_DIR is not set)
endif

.PHONY: libc
libc: $(OUTPUT_DIR)/libc.a

#Pull in dependency info for *existing* .o files
-include $(DEPS)

$(OUTPUT_DIR)/libc.a: $(OBJS)
	$(AR) $(ARFLAGS) -o $@ $^

$(OUTPUT_DIR)/crt0.o: start.S
	@mkdir -p $(@D)
	$(AS) -64 -o $@ $<

$(OUTPUT_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -MT $@ -c $< -o $@

$(OUTPUT_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(AS) -64 -o $@ $<

.PHONY: clean
clean:
	-$(RM) $(OUTPUT_DIR)
