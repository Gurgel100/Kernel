
CC = $(CROSSTOOL_PREFIX)gcc
LD = $(CROSSTOOL_PREFIX)gcc
AS = $(CROSSTOOL_PREFIX)as
RM = rm -rf

OUTPUT_DIR = build

CPPFLAGS += -I"./include" -I"./cdi" -I"./driver" -I"./Include" -I"./loader" -I"./mm" -I"./tasks" -I"./util" -I"."
CFLAGS += -gdwarf-4 -Wall -Wextra -fmessage-length=0 -ffreestanding -fno-stack-protector -mno-red-zone -fno-omit-frame-pointer -std=gnu99 -mcx16 -mno-sse
LDFLAGS += -nostdlib -static -T./kernel.ld -z max-page-size=0x1000
C_SRCS = $(shell find -name '*.c' ! -path './lib/stdlibc/math.c')
S_SRCS = ./interrupts.S ./start.S

C_OBJS = $(patsubst ./%,$(OUTPUT_DIR)/%,$(C_SRCS:.c=.o))
S_OBJS = $(patsubst ./%,$(OUTPUT_DIR)/%,$(S_SRCS:.S=.o))
OBJS := $(C_OBJS) $(S_OBJS)
DEPS := $(OBJS:.o=.d)

ifeq ($(BUILD_CONFIG), release)
	CFLAGS += -O3
else
	CFLAGS += -Og
endif


#get git information
DIRTY=
ifneq ($(shell git diff --shortstat 2> /dev/null),)
	DIRTY=-dirty
endif
GIT_COMMIT_ID=$(shell git rev-parse HEAD)$(DIRTY)
GIT_BRANCH=$(shell git rev-parse --abbrev-ref HEAD)

.PHONY: all
all: kernel libc

.PHONY: release
release:
	$(MAKE) BUILD_CONFIG=$@

.PHONY: install-headers
install-headers:
	$(MAKE) -C lib install-headers

.PHONY: install-libc
install-libc:
	$(MAKE) -C lib install

.PHONY: libc
libc:
	$(MAKE) BUILD_CONFIG=$(BUILD_CONFIG) -C lib

.PHONY: kernel
kernel: $(OUTPUT_DIR)/kernel

#Pull in dependency info for *existing* .o files
-include $(DEPS)

$(OUTPUT_DIR)/kernel: CPPFLAGS += -DBUILD_KERNEL
$(OUTPUT_DIR)/kernel: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(OUTPUT_DIR)/driver/ext2/%: CPPFLAGS += -I"./driver/ext2/libext2/include"

$(OUTPUT_DIR)/version.o: force_build
$(OUTPUT_DIR)/version.o: CPPFLAGS += -DGIT_BRANCH=$(GIT_BRANCH) -DGIT_COMMIT_ID=$(GIT_COMMIT_ID)

$(OUTPUT_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) -MMD -MP -MT $@ -c $< -o $@

$(OUTPUT_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(AS) -64 --defsym BUILD_KERNEL= -o $@ $<

.PHONY: clean
clean: clean-kernel clean-libc

.PHONY: clean-kernel
clean-kernel:
	-$(RM) $(OUTPUT_DIR)

.PHONY: clean-libc
clean-libc:
	-$(MAKE) -C lib clean
	
force_build:
